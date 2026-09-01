#!/usr/bin/env ruby

# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

require "digest"
require "fileutils"
require "shellwords"
require "tmpdir"

obs_version = "29.1.3"
repository_dir = File.expand_path("../..", __dir__)
output_dir = File.join(repository_dir, "vendor", "sdk", "obs-#{obs_version}-macos")
work_dir = Dir.mktmpdir("obs-macos-tbd.", ENV.fetch("TMPDIR", "/tmp"))
mount_points = []
generated_tbds = Hash.new { |hash, key| hash[key] = [] }

at_exit do
  mount_points.reverse_each do |mount_point|
    system("hdiutil", "detach", mount_point, out: File::NULL, err: File::NULL)
  end
  FileUtils.remove_entry(work_dir) if File.exist?(work_dir)
end

architectures = %w[arm64 x86_64]

architectures.each do |architecture|
  case architecture
  when "arm64"
    dmg_sha256 = "ad8586d6af8dd4a0039e6074cf92213340f3d2408cf87e3593fa0822cbc8a73a"
  when "x86_64"
    dmg_sha256 = "0e87051cd5ee50f9efb9c9052d79a3d598761b154308213c40accacc3c9d0895"
  else
    abort "ERROR: unsupported architecture: #{architecture}\nUsage: #{File.basename($PROGRAM_NAME)} [arm64|x86_64 ...]"
  end

  dmg_name = "obs-studio-#{obs_version}-macos-#{architecture}.dmg"
  dmg_url = "https://github.com/obsproject/obs-studio/releases/download/#{obs_version}/#{dmg_name}"
  download_dir = ENV.fetch("OBS_SDK_DOWNLOAD_DIR", work_dir)
  dmg_path = File.join(download_dir, dmg_name)
  mount_point = File.join(work_dir, "mount-#{architecture}")
  staging_dir = File.join(work_dir, "output-#{architecture}")

  FileUtils.mkdir_p([download_dir, mount_point, staging_dir])

  unless File.file?(dmg_path)
    system("curl", "-fL", "--retry", "3", "-o", dmg_path, dmg_url) or
      abort "ERROR: failed to download #{dmg_url}"
  end

  unless Digest::SHA256.file(dmg_path).hexdigest == dmg_sha256
    abort "ERROR: checksum verification failed for #{dmg_path}"
  end

  system("hdiutil", "attach", "-quiet", "-readonly", "-nobrowse", "-mountpoint", mount_point, dmg_path) or
    abort "ERROR: failed to mount #{dmg_path}"
  mount_points << mount_point

  frameworks = File.join(mount_point, "OBS.app", "Contents", "Frameworks")
  libraries = {
    "libobs.tbd" => File.join(frameworks, "libobs.framework", "Versions", "A", "libobs"),
    "obs-frontend-api.tbd" => File.join(frameworks, "obs-frontend-api.dylib")
  }
  symbol_filters = {
    "libobs.tbd" => [
      /\A_obs_/,
      /\A_gs_/,
      /\A_config_/,
      /\A_os_/,
      /\A_text_lookup_/,
      /\A_audio_(?:output|resampler)_/,
      /\A_video_(?:format|output)_/,
      /\A_signal_handler_/,
      /\A_proc_handler_/,
      /\A_dstr_/,
      /\A_(?:matrix4|quat|axisang|vec2|vec3|vec4)_/,
      /\A_profile(?:r)?_/,
      /\A_(?:array_output|file_input|file_output)_serializer_/,
      /\A_(?:astrcmp_n|astrcmpi|astrcmpi_n|astrstri|base_set_crash_handler|base_set_log_handler|bcrash|bfree|blog|blogva|bmalloc|bmemdup|bnum_allocs|brealloc|bstrdup|bstrdup_n|bzalloc|rand_float|strdepad|strlist_free|strlist_split|wcsdepad|wstrcmp_n|wstrcmpi|wstrcmpi_n|wstrstri)\z/
    ],
    "obs-frontend-api.tbd" => [/\A_obs_frontend_/]
  }

  libraries.each do |tbd_name, library|
    actual_architecture = `lipo -archs #{library.shellescape}`.strip
    abort "ERROR: failed to inspect #{library}" unless $?.success?
    unless actual_architecture == architecture
      abort "ERROR: #{library} architecture is #{actual_architecture}, expected #{architecture}"
    end

    raw_tbd = File.join(staging_dir, "raw-#{tbd_name}")
    edited_tbd = File.join(staging_dir, "edited-#{tbd_name}")
    formatted_tbd = File.join(staging_dir, tbd_name)

    system("xcrun", "tapi", "stubify", "--filetype=tbd-v4", "-o", raw_tbd, library) or
      abort "ERROR: failed to stubify #{library}"

    filters = symbol_filters.fetch(tbd_name)
    yaml = File.read(raw_tbd, encoding: "UTF-8")
    matched_blocks = 0
    yaml.gsub!(/^(\s*symbols:\s*)\[(.*?)\]/m) do
      matched_blocks += 1
      symbols = Regexp.last_match(2).split(",").map(&:strip).reject(&:empty?)
      symbols.select! { |symbol| filters.any? { |filter| filter.match?(symbol) } }
      abort "ERROR: symbol filter removed every symbol from #{raw_tbd}" if symbols.empty?
      "#{Regexp.last_match(1)}[ #{symbols.join(", ")} ]"
    end

    abort "ERROR: no symbol blocks were found in #{raw_tbd}" if matched_blocks.zero?

    yaml << "...\n" unless yaml.match?(/(?:\A|\n)\.\.\.\s*\z/)
    File.write(edited_tbd, yaml, encoding: "UTF-8")

    system("xcrun", "tapi", "archive", "--extract", architecture, edited_tbd, "-o", formatted_tbd) or
      abort "ERROR: failed to format #{edited_tbd}"
    system("xcrun", "tapi", "archive", "--verify-arch", architecture, formatted_tbd) or
      abort "ERROR: failed to verify #{formatted_tbd}"

    generated_tbds[tbd_name] << formatted_tbd
  end
end

FileUtils.mkdir_p(File.join(output_dir, "lib"))
generated_tbds.each do |tbd_name, tbds|
  merged_tbd = File.join(work_dir, tbd_name)
  system("xcrun", "tapi", "archive", "--merge", *tbds, "-o", merged_tbd) or
    abort "ERROR: failed to merge #{tbd_name}"
  architectures.each do |architecture|
    system("xcrun", "tapi", "archive", "--verify-arch", architecture, merged_tbd) or
      abort "ERROR: failed to verify #{architecture} in #{merged_tbd}"
  end
  FileUtils.install(merged_tbd, File.join(output_dir, "lib", tbd_name), mode: 0o644)
end
