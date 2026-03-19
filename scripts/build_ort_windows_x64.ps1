# SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
# SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: GPL-3.0-or-later

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ORT_VERSION = "v1.24.1"
$CONFIGURATION = "Release"
$DML_PACKAGE_VERSION = "1.15.4"
$ORT_RUNTIME_DLL_NAMES = @(
"DirectML.dll",
"onnxruntime_providers_shared.dll",
"onnxruntime_providers_cuda.dll",
"onnxruntime_providers_tensorrt.dll"
)
$ORT_RUNTIME_LIB_NAMES = @(
"DirectML.lib"
)
$ORT_COMPONENTS = @(
"onnxruntime_session",
"onnxruntime_optimizer",
"onnxruntime_providers",
    "onnxruntime_providers_dml",
"onnxruntime_lora",
"onnxruntime_framework",
"onnxruntime_graph",
"onnxruntime_util",
"onnxruntime_mlas",
"onnxruntime_common",
"onnxruntime_flatbuffers"
)

$ROOT_DIR = Convert-Path .
$DEPS_DIR = Join-Path $ROOT_DIR ".deps_vendor"
if (!(Test-Path $DEPS_DIR)) { New-Item -ItemType Directory -Path $DEPS_DIR | Out-Null }
$ORT_SRC_DIR = Join-Path $DEPS_DIR "onnxruntime"
$DML_X64_DIR = Join-Path $DEPS_DIR "ort_x64\packages\Microsoft.AI.DirectML.$DML_PACKAGE_VERSION\bin\x64-win"
$BUILD_PY = Join-Path $ORT_SRC_DIR "tools\ci_build\build.py"
$ORT_BUILD_DIR = Join-Path $DEPS_DIR "ort_x64"
$WRAPPER_DIR = Join-Path $DEPS_DIR "wrapper"
if (!(Test-Path $WRAPPER_DIR)) { New-Item -ItemType Directory -Path $WRAPPER_DIR | Out-Null }
$WRAPPER_CL_EXE = Join-Path $WRAPPER_DIR "cl.exe"

$CCACHE_COMMAND = Get-Command ccache.exe -ErrorAction SilentlyContinue
$VsGlobals = "CMAKE_VS_GLOBALS=UseMultiToolTask=true;EnforceProcessCountAcrossBuilds=true;TrackFileAccess=false"

if ($CCACHE_COMMAND) {
if (Test-Path $WRAPPER_CL_EXE) {
Remove-Item -Path $WRAPPER_CL_EXE -Force -ErrorAction SilentlyContinue
}
Copy-Item -Path $CCACHE_COMMAND.Source -Destination $WRAPPER_CL_EXE
$VsGlobals = "$VsGlobals;CLToolExe=cl.exe;CLToolPath=$WRAPPER_DIR"
}

if (!(Test-Path $ORT_SRC_DIR)) {
git clone --depth 1 --branch $ORT_VERSION https://github.com/microsoft/onnxruntime.git $ORT_SRC_DIR
if ($LASTEXITCODE -ne 0) { throw "git clone failed" }

try {
Push-Location $ORT_SRC_DIR
git submodule update --init --recursive --depth 1
if ($LASTEXITCODE -ne 0) { throw "git submodule update failed" }

(Get-Content $BUILD_PY) -replace 'cmake_args \+= \["-DCMAKE_VS_GLOBALS=UseMultiToolTask=true;EnforceProcessCountAcrossBuilds=true"\]', 'pass' | Set-Content $BUILD_PY
} finally {
Pop-Location
}
}

$commonArgs = @(
"--build_dir", "$ORT_BUILD_DIR",
"--config", "$CONFIGURATION",
"--parallel",
"--compile_no_warning_as_error",
"--cmake_extra_defines",
"CMAKE_POLICY_VERSION_MINIMUM=3.5",
$VsGlobals,
"--use_cache",
"--use_vcpkg",
"--skip_submodule_sync",
"--skip_tests","--disable_rtti",
"--use_dml",
"--targets"
)

$commonArgs += $ORT_COMPONENTS

if (!(Test-Path $ORT_BUILD_DIR)) {
& python $BUILD_PY --update @commonArgs
if ($LASTEXITCODE -ne 0) { throw "build.py update failed" }
}

& python $BUILD_PY --build @commonArgs
if ($LASTEXITCODE -ne 0) { throw "build.py build failed" }

$LIB_DIR = Join-Path $DEPS_DIR "lib"
if (!(Test-Path $LIB_DIR)) { New-Item -ItemType Directory -Path $LIB_DIR | Out-Null }
$BIN_DIR = Join-Path $DEPS_DIR "bin"
if (!(Test-Path $BIN_DIR)) { New-Item -ItemType Directory -Path $BIN_DIR | Out-Null }

foreach ($name in $ORT_COMPONENTS) {
$sourcePath = Join-Path $ORT_BUILD_DIR $CONFIGURATION $CONFIGURATION "${name}.lib"
Copy-Item -Path $sourcePath -Destination $LIB_DIR -Force
}

foreach ($name in $ORT_RUNTIME_LIB_NAMES) {
$sourcePath = Join-Path $DML_X64_DIR $name
if ($sourcePath) {
Copy-Item -Path $sourcePath -Destination (Join-Path $LIB_DIR $name) -Force
}
}

foreach ($name in $ORT_RUNTIME_DLL_NAMES) {
$sourcePath = if ($name -eq "DirectML.dll") {
Join-Path $DML_X64_DIR $name
} else {
Get-ChildItem -Path $ORT_BUILD_DIR, $ORT_SRC_DIR -Filter $name -File -Recurse -ErrorAction SilentlyContinue |
Select-Object -First 1 -ExpandProperty FullName
}
if ($sourcePath) {
Copy-Item -Path $sourcePath -Destination (Join-Path $BIN_DIR $name) -Force
}
}

