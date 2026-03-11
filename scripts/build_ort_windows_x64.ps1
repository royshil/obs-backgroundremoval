# SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
# SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: GPL-3.0-or-later

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ORT_VERSION = "v1.24.1"
$CONFIGURATION = "Release"
$ORT_COMPONENTS = @(
	"onnxruntime_session",
	"onnxruntime_optimizer",
	"onnxruntime_providers",
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
if (!(Test-Path $DEPS_DIR)) { New-Item -ItemType Directory -Path $DEPS_DIR }
$ORT_SRC_DIR = Join-Path $DEPS_DIR "onnxruntime"
$BUILD_PY = Join-Path $ORT_SRC_DIR "tools\ci_build\build.py"
$ORT_BUILD_DIR = Join-Path $DEPS_DIR "ort_x64"
$WRAPPER_DIR = Join-Path $DEPS_DIR "wrapper"
if (!(Test-Path $WRAPPER_DIR)) { New-Item -ItemType Directory -Path $WRAPPER_DIR }
$WRAPPER_CL_EXE = Join-Path $WRAPPER_DIR "cl.exe"

$CCACHE_PROGRAM_PATH = (Get-Command ccache.exe).Source

if (Test-Path $WRAPPER_CL_EXE) {
	Remove-Item -Path $WRAPPER_CL_EXE -Force -ErrorAction SilentlyContinue
}
Copy-Item -Path $CCACHE_PROGRAM_PATH -Destination $WRAPPER_CL_EXE

if (!(Test-Path $ORT_SRC_DIR)) {
	git clone --depth 1 --branch $ORT_VERSION https://github.com/microsoft/onnxruntime.git $ORT_SRC_DIR
	if ($LASTEXITCODE -ne 0) { throw "git clone failed" }

	try {
		Push-Location $ORT_SRC_DIR
		git submodule update --init --recursive --depth 1
		if ($LASTEXITCODE -ne 0) { throw "git submodule update failed" }

		(Get-Content $BUILD_PY) -replace 'cmake_args \+= \["-DCMAKE_VS_GLOBALS=UseMultiToolTask=true;EnforceProcessCountAcrossBuilds=true"\]', 'pass' | Set-Content $BUILD_PY
	} catch {
		throw
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
	"CMAKE_VS_GLOBALS=UseMultiToolTask=true;EnforceProcessCountAcrossBuilds=true;TrackFileAccess=false;CLToolExe=cl.exe;CLToolPath=$WRAPPER_DIR",
	"--use_cache",
	"--use_vcpkg",
	"--skip_submodule_sync",
	"--skip_tests",
	"--include_ops_by_config", "$ROOT_DIR/data/models/required_operators_and_types.with_runtime_opt.config",
	"--enable_reduced_operator_type_support",
	"--disable_rtti",
	"--targets"
)

$commonArgs += $ORT_COMPONENTS

if (!(Test-Path $ORT_BUILD_DIR)) {
	try {
		& python $BUILD_PY --update @commonArgs
		if ($LASTEXITCODE -ne 0) { throw "build.py update failed" }
	} catch {
		throw
	}
}

& python $BUILD_PY --build @commonArgs
if ($LASTEXITCODE -ne 0) { throw "build.py build failed" }

$LIB_DIR = Join-Path $DEPS_DIR "lib"
if (!(Test-Path $LIB_DIR)) { New-Item -ItemType Directory -Path $LIB_DIR }

foreach ($name in $ORT_COMPONENTS) {
	$sourcePath = Join-Path $ORT_BUILD_DIR $CONFIGURATION $CONFIGURATION "${name}.lib"
	Copy-Item -Path $sourcePath -Destination $LIB_DIR -Force
}
