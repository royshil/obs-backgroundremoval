# SPDX-FileCopyrightText: 2025-2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# ------------------------------------------------------------------------------
# 1. Setup Phase
# ------------------------------------------------------------------------------
Write-Host "::group::Initialize and Check Dependencies"

$ScriptDir = $PSScriptRoot
$RootDir = Split-Path -Parent $ScriptDir
$DepsDir = Join-Path $RootDir ".deps"
$BuildSpecFile = Join-Path $RootDir "buildspec.json"

if (-not (Test-Path $BuildSpecFile)) {
    Write-Error "buildspec.json not found at $BuildSpecFile"
    exit 1
}

try {
    $Spec = Get-Content $BuildSpecFile -Raw | ConvertFrom-Json
}
catch {
    Write-Error "Failed to parse buildspec.json: $_"
    exit 1
}

$ObsVersion = $Spec.dependencies."obs-studio".version
$PrebuiltVersion = $Spec.dependencies.prebuilt.version
$Qt6Version = $Spec.dependencies.qt6.version

Write-Host "Detected Versions from buildspec.json:"
Write-Host "  OBS Studio: $ObsVersion"
Write-Host "  Prebuilt:   $PrebuiltVersion"
Write-Host "  Qt6:        $Qt6Version"

$PrebuiltDir = Join-Path $DepsDir "obs-deps-$PrebuiltVersion-x64"
$Qt6Dir = Join-Path $DepsDir "obs-deps-qt6-$Qt6Version-x64"
$SourceDir = Join-Path $DepsDir "obs-studio-$ObsVersion"
$BuildDir = Join-Path $SourceDir "build_x64"

if (-not (Test-Path $SourceDir)) {
    Write-Error "Error: OBS source directory not found at $SourceDir"
    Write-Error "Please run 'cmake -P scripts/download_deps.cmake' first."
    exit 1
}

Write-Host "Cleaning build directory..."
if (Test-Path $BuildDir) {
    Remove-Item -Path $BuildDir -Recurse -Force
}

Write-Host "::endgroup::"

# ------------------------------------------------------------------------------
# 2. Configure Phase
# ------------------------------------------------------------------------------
Write-Host "::group::Configure OBS Studio (x64)"

$CmakeArgs = @(
    "-S", "$SourceDir",
    "-B", "$BuildDir",
    "-G", "Visual Studio 17 2022",
    "-A", "x64",
    "-DOBS_CMAKE_VERSION=3.0.0",
    "-DENABLE_PLUGINS=OFF",
    "-DENABLE_FRONTEND=OFF",
    "-DOBS_VERSION_OVERRIDE=$ObsVersion",
    "-DCMAKE_PREFIX_PATH=$PrebuiltDir;$Qt6Dir",
    "-DCMAKE_INSTALL_PREFIX=$DepsDir"
)

cmake @CmakeArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "::endgroup::"

# ------------------------------------------------------------------------------
# 3. Build Phase (Debug)
# ------------------------------------------------------------------------------
Write-Host "::group::Build OBS Frontend API (Debug)"

cmake --build "$BuildDir" --target obs-frontend-api --config Debug --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "::endgroup::"

# ------------------------------------------------------------------------------
# 4. Build Phase (Release)
# ------------------------------------------------------------------------------
Write-Host "::group::Build OBS Frontend API (Release)"

cmake --build "$BuildDir" --target obs-frontend-api --config Release --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "::endgroup::"

# ------------------------------------------------------------------------------
# 5. Install Phase (Debug)
# ------------------------------------------------------------------------------
Write-Host "::group::Install Development Artifacts (Debug)"

cmake --install "$BuildDir" --component Development --config Debug --prefix "$DepsDir"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "::endgroup::"

# ------------------------------------------------------------------------------
# 6. Install Phase (Release)
# ------------------------------------------------------------------------------
Write-Host "::group::Install Development Artifacts (Release)"

cmake --install "$BuildDir" --component Development --config Release --prefix "$DepsDir"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Install done. Artifacts are in $DepsDir"
Write-Host "::endgroup::"
