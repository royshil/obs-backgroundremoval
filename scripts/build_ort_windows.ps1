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

if (!(Test-Path $ORT_SRC_DIR)) {
	try {
		git clone --depth 1 --branch $ORT_VERSION https://github.com/microsoft/onnxruntime.git $ORT_SRC_DIR
		Set-Location $ORT_SRC_DIR
		git submodule update --init --recursive --depth 1
	} catch {
		throw
	} finally {
		Set-Location $ROOT_DIR
	}
}

$BUILD_PY = Join-Path $ORT_SRC_DIR "tools\ci_build\build.py"
$ORT_BUILD_DIR = Join-Path $DEPS_DIR "ort_x64"

$commonArgs = @(
	"--build_dir", "$ORT_BUILD_DIR",
	"--config", "$CONFIGURATION",
	"--parallel",
	"--compile_no_warning_as_error",
	"--cmake_extra_defines", "CMAKE_POLICY_VERSION_MINIMUM=3.5",
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
	} catch {
		Remove-Item -Path $ORT_BUILD_DIR -Recurse -Force
		throw
	}
}

& python $BUILD_PY --build @commonArgs

$LIB_DIR = Join-Path $DEPS_DIR "lib"
if (!(Test-Path $LIB_DIR)) { New-Item -ItemType Directory -Path $LIB_DIR }

foreach ($name in $ORT_COMPONENTS) {
	$sourcePath = Join-Path $ORT_BUILD_DIR -ChildPath $CONFIGURATION -AdditionalChildPath $CONFIGURATION, "${name}.lib"
	Copy-Item -Path $sourcePath -Destination $LIB_DIR -Force
}
