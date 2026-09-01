# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true

$OBS_VERSION = '29.1.3'
$OBS_ZIP_SHA256 = '45d7ea580137fe50a19bdc62e12dcfc1406d74c51341308010d7b6d197339524'
$REPOSITORY_DIR = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$OUTPUT_DIR = Join-Path $REPOSITORY_DIR "vendor\sdk\obs-$OBS_VERSION-windows\lib"
$WORK_DIR = Join-Path ([System.IO.Path]::GetTempPath()) "obs-windows-import-libs.$([guid]::NewGuid())"
$DOWNLOAD_DIR = if ($env:OBS_SDK_DOWNLOAD_DIR) { $env:OBS_SDK_DOWNLOAD_DIR } else { $WORK_DIR }
$OBS_ZIP_NAME = "OBS-Studio-$OBS_VERSION.zip"
$OBS_ZIP_URL = "https://github.com/obsproject/obs-studio/releases/download/$OBS_VERSION/$OBS_ZIP_NAME"
$OBS_ZIP_PATH = Join-Path $DOWNLOAD_DIR $OBS_ZIP_NAME
$EXTRACT_DIR = Join-Path $WORK_DIR 'obs'

New-Item -ItemType Directory -Force -Path $DOWNLOAD_DIR, $EXTRACT_DIR, $OUTPUT_DIR | Out-Null

try {
  if (-not (Test-Path -LiteralPath $OBS_ZIP_PATH -PathType Leaf)) {
    curl.exe -fL --retry 3 -o $OBS_ZIP_PATH $OBS_ZIP_URL
  }

  if ((Get-FileHash -Algorithm SHA256 -LiteralPath $OBS_ZIP_PATH).Hash -ne $OBS_ZIP_SHA256) {
    throw "Checksum verification failed for $OBS_ZIP_PATH"
  }

  Expand-Archive -LiteralPath $OBS_ZIP_PATH -DestinationPath $EXTRACT_DIR

  $DUMPBIN_COMMAND = (Get-Command dumpbin.exe -ErrorAction Stop).Source
  $LIB_COMMAND = (Get-Command lib.exe -ErrorAction Stop).Source
  $LIBRARIES = [ordered]@{
    'obs.lib' = 'obs.dll'
    'obs-frontend-api.lib' = 'obs-frontend-api.dll'
  }

  foreach ($LIBRARY in $LIBRARIES.GetEnumerator()) {
    $DLL_PATH = Join-Path $EXTRACT_DIR "bin\64bit\$($LIBRARY.Value)"
    $DEF_PATH = Join-Path $WORK_DIR "$($LIBRARY.Value).def"
    $GENERATED_LIB_PATH = Join-Path $WORK_DIR $LIBRARY.Key
    $LIB_PATH = Join-Path $OUTPUT_DIR $LIBRARY.Key
    $DUMPBIN_OUTPUT = & $DUMPBIN_COMMAND /exports $DLL_PATH

    if ($LASTEXITCODE -ne 0) {
      throw "dumpbin failed for $DLL_PATH"
    }

    $SYMBOLS = @(
      foreach ($LINE in $DUMPBIN_OUTPUT) {
        if ($LINE -match '^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+(\S+)') {
          $Matches[1]
        }
      }
    )

    if ($SYMBOLS.Count -eq 0) {
      throw "No exported symbols were found in $DLL_PATH"
    }

    @(
      "LIBRARY `"$($LIBRARY.Value)`""
      'EXPORTS'
      $SYMBOLS | ForEach-Object { "  $_" }
    ) | Set-Content -LiteralPath $DEF_PATH -Encoding ascii

    & $LIB_COMMAND /nologo /Brepro /machine:x64 "/def:$DEF_PATH" "/out:$GENERATED_LIB_PATH"

    if ($LASTEXITCODE -ne 0) {
      throw "lib.exe failed for $DEF_PATH"
    }

    Copy-Item -LiteralPath $GENERATED_LIB_PATH -Destination $LIB_PATH -Force
    Write-Host "Generated $LIB_PATH ($($SYMBOLS.Count) exports)"
  }
} finally {
  Remove-Item -LiteralPath $WORK_DIR -Recurse -Force -ErrorAction SilentlyContinue
}
