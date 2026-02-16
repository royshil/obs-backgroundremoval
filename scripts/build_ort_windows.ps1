Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ORT_VERSION = "v1.24.1"
$CONFIGURATION = "Release"
# $ORT_COMPONENTS = @( "onnxruntime_session", "onnxruntime_optimizer", "onnxruntime_providers", "onnxruntime_lora", "onnxruntime_framework", "onnxruntime_graph", "onnxruntime_util", "onnxruntime_mlas", "onnxruntime_common", "onnxruntime_flatbuffers", "onnxruntime_providers_dml", "onnxruntime_providers_cuda" )

$ROOT_DIR = Convert-Path .
$DEPS_DIR = Join-Path $ROOT_DIR ".deps_vendor"
if (!(Test-Path $DEPS_DIR)) { New-Item -ItemType Directory -Path $DEPS_DIR }
$ORT_SRC_DIR = Join-Path $DEPS_DIR "onnxruntime"

if (!(Test-Path $ORT_SRC_DIR)) {
	git clone --depth 1 --branch $ORT_VERSION https://github.com/microsoft/onnxruntime.git $ORT_SRC_DIR
	Set-Location $ORT_SRC_DIR
	git submodule update --init --recursive --depth 1
}

$BUILD_PY = Join-Path $ORT_SRC_DIR "tools\ci_build\build.py"
$ORT_BUILD_DIR = Join-Path $DEPS_DIR "ort_x64"
if (!(Test-Path $ORT_BUILD_DIR)) {
	& python3 $BUILD_PY `
		--build_dir "$ORT_BUILD_DIR" `
		--config $CONFIGURATION `
		--update `
		--parallel `
		--compile_no_warning_as_error `
		--cmake_extra_defines CMAKE_POLICY_VERSION_MINIMUM=3.5 `
		--use_vcpkg `
		--skip_submodule_sync `
		--skip_tests `
		--include_ops_by_config "$ROOT_DIR/data/models/required_operators_and_types.with_runtime_opt.config" `
		--enable_reduced_operator_type_support `
		--disable_rtti
}

& python3 $BUILD_PY `
	--build_dir "$ORT_BUILD_DIR" `
	--config $CONFIGURATION `
	--update `
	--parallel `
	--compile_no_warning_as_error `
	--cmake_extra_defines CMAKE_POLICY_VERSION_MINIMUM=3.5 `
	--use_vcpkg `
	--skip_submodule_sync `
	--skip_tests `
	--include_ops_by_config "$ROOT_DIR/data/models/required_operators_and_types.with_runtime_opt.config" `
	--enable_reduced_operator_type_support `
	--disable_rtti
