# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

# Updates the curl import library that OBS Studio ships through obs-deps. The curl headers come from
# the vendor/curl/curl submodule, which MUST be at the commit that this obs-deps release builds:
# curl-8_7_1 (de7b3e89218467159a7af72d58cea8425946e97d), from deps.windows/30-curl.ps1 at the
# release.
#
# Usage: pwsh vendor/obs-deps/update-obs-deps.ps1

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true

# The obs-deps release OBS Studio 30.2.3 is built with.
$OBS_DEPS_VERSION = '2024-05-08'
$OBS_DEPS_SHA256 = '773c87a0d173125ef2768aaca6f6de64a7d6053213cc9f7d3970a301152042d8'

$LIB_DIR = Join-Path $PSScriptRoot 'lib\x64-windows'
$WORK_DIR = Join-Path $PSScriptRoot '..\..\build\update-obs-deps'

Remove-Item -LiteralPath $WORK_DIR -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $WORK_DIR, $LIB_DIR | Out-Null

try {
	$OBS_DEPS_ZIP = Join-Path $WORK_DIR 'obs-deps.zip'
	curl.exe -fsSL --retry 3 -o $OBS_DEPS_ZIP `
		"https://github.com/obsproject/obs-deps/releases/download/$OBS_DEPS_VERSION/windows-deps-$OBS_DEPS_VERSION-x64.zip"
	if ((Get-FileHash -Algorithm SHA256 -LiteralPath $OBS_DEPS_ZIP).Hash -ne $OBS_DEPS_SHA256) {
		throw 'Checksum verification failed for obs-deps'
	}
	Expand-Archive -LiteralPath $OBS_DEPS_ZIP -DestinationPath (Join-Path $WORK_DIR 'obs-deps')
	Copy-Item -LiteralPath (Join-Path $WORK_DIR 'obs-deps\lib\libcurl_imp.lib') -Destination $LIB_DIR -Force
} finally {
	Remove-Item -LiteralPath $WORK_DIR -Recurse -Force -ErrorAction SilentlyContinue
}
