# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

if(NOT DEPS_DIR)
  set(DEPS_DIR "${CMAKE_SOURCE_DIR}/.deps")
endif()

file(STRINGS "${CMAKE_SOURCE_DIR}/buildspec.props" BUILDSPEC_LINES)
foreach(LINE IN LISTS BUILDSPEC_LINES)
  if(LINE MATCHES "^([a-zA-Z][a-zA-Z0-9_]*)=(.*)$")
    set("${CMAKE_MATCH_1}" "${CMAKE_MATCH_2}")
  endif()
endforeach()

function(download_dep DEP_NAME URL SHA256)
  get_filename_component(FILENAME "${URL}" NAME)

  file(
    DOWNLOAD "${URL}"
    "${DEPS_DIR}/${FILENAME}"
    EXPECTED_HASH "SHA256=${SHA256}"
    STATUS download_status
    TLS_VERIFY ON
  )

  list(GET download_status 0 status_code)
  list(GET download_status 1 status_message)
  if(NOT status_code EQUAL 0)
    message(FATAL_ERROR "Download failed: ${status_message}")
  endif()

  file(ARCHIVE_EXTRACT INPUT "${DEPS_DIR}/${FILENAME}" DESTINATION "${DEPS_DIR}/${DEP_NAME}")
endfunction()

function(set_output NAME VALUE)
  message("${NAME}=${VALUE}")
  if(DEFINED ENV{GITHUB_OUTPUT})
    file(APPEND "$ENV{GITHUB_OUTPUT}" "${NAME}<<EOS\n${VALUE}\nEOS\n")
  endif()
endfunction()

if(WIN32)
  if(ccache_windows_url MATCHES "-([0-9]+\.[0-9]+\.[0-9]+)-")
    set(CCACHE_VERSION "${CMAKE_MATCH_1}")
  else()
    message(FATAL_ERROR "ERROR: Failed to extract ccache version")
  endif()

  file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/.deps")

  download_dep(obs-deps "${prebuilt_windows_x64_url}" "${prebuilt_windows_x64_sha256}")
  file(
    WRITE "${CMAKE_SOURCE_DIR}/vendor/obs-studio/.deps/.dependency_prebuilt_x64.sha256"
    "${prebuilt_windows_x64_sha256}"
  )
  set_output(OBS_DEPS_PREFIX "${CMAKE_SOURCE_DIR}/.deps/obs-deps")

  download_dep(obs-deps-qt6 "${qt6_windows_x64_url}" "${qt6_windows_x64_sha256}")
  file(WRITE "${CMAKE_SOURCE_DIR}/vendor/obs-studio/.deps/.dependency_qt6_x64.sha256" "${qt6_windows_x64_sha256}")
  set_output(OBS_DEPS_QT6_PREFIX "${CMAKE_SOURCE_DIR}/.deps/obs-deps-qt6")

  download_dep(ccache "${ccache_windows_url}" "${ccache_windows_sha256}")
  set_output(CCACHE_COMMAND "${CMAKE_SOURCE_DIR}/.deps/ccache/ccache-${CCACHE_VERSION}-windows-x86_64/ccache.exe")
  set_output(CCACHE_VERSION "${CCACHE_VERSION}")
elseif(APPLE)
  if(ccache_macos_url MATCHES "-([0-9]+\.[0-9]+\.[0-9]+)-")
    set(CCACHE_VERSION "${CMAKE_MATCH_1}")
  else()
    message(FATAL_ERROR "Failed to extract ccache version")
  endif()

  file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/.deps" "${CMAKE_SOURCE_DIR}/vendor/obs-studio/.deps")

  download_dep(obs-deps "${prebuilt_macos_url}" "${prebuilt_macos_sha256}")
  file(
    WRITE "${CMAKE_SOURCE_DIR}/vendor/obs-studio/.deps/.dependency_prebuilt_universal.sha256"
    "${prebuilt_macos_sha256}"
  )
  set_output(OBS_DEPS_PREFIX "${CMAKE_SOURCE_DIR}/.deps/obs-deps")

  download_dep(obs-deps-qt6 "${qt6_macos_url}" "${qt6_macos_sha256}")
  file(WRITE "${CMAKE_SOURCE_DIR}/vendor/obs-studio/.deps/.dependency_qt6_universal.sha256" "${qt6_macos_sha256}")
  set_output(OBS_DEPS_QT6_PREFIX "${CMAKE_SOURCE_DIR}/.deps/obs-deps-qt6")

  download_dep(ccache "${ccache_macos_url}" "${ccache_macos_sha256}")
  set_output(CCACHE_COMMAND "${CMAKE_SOURCE_DIR}/.deps/ccache/ccache-${CCACHE_VERSION}-darwin/ccache")
  set_output(CCACHE_VERSION "${CCACHE_VERSION}")
else()
  message(FATAL_ERROR "Platform not supported")
endif()
