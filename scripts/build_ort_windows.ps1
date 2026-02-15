lsSet-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# --- Configuration ---
$ORT_VERSION = "v1.24.1"
$CONFIGURATION = "Release"
$ORT_COMPONENTS = @( "onnxruntime_session", "onnxruntime_optimizer", "onnxruntime_providers", "onnxruntime_lora", "onnxruntime_framework", "onnxruntime_graph", "onnxruntime_util", "onnxruntime_mlas", "onnxruntime_common", "onnxruntime_flatbuffers", "onnxruntime_providers_dml", "onnxruntime_providers_cuda" )

$ROOT_DIR = Convert-Path .
$DEPS_DIR = Join-Path $ROOT_DIR ".deps_vendor"
if (!(Test-Path $DEPS_DIR)) { New-Item -ItemType Directory -Path $DEPS_DIR | Out-Null }
Set-Location $DEPS_DIR

# --- 1. Clone ONNX Runtime repository ---
if (Test-Path "onnxruntime") {
	Set-Location "onnxruntime"
} else {
	git clone --depth 1 --branch $ORT_VERSION https://github.com/microsoft/onnxruntime.git
	Set-Location "onnxruntime"
	git submodule update --init --recursive --depth 1
	Copy-Item cmake\CMakeLists.txt cmake\CMakeLists.txt.orig
	"macro(install)`nendmacro()`n$(Get-Content cmake\CMakeLists.txt.orig -Raw)" | Set-Content cmake\CMakeLists.txt -NoNewline
}

# --- 2. Build ONNX Runtime for Windows x64 (CPU, DirectML, CUDA) ---
$ORT_BUILD_DIR = Join-Path $DEPS_DIR "ort_x64"
if (!(Test-Path $ORT_BUILD_DIR)) {
	& python tools/ci_build/build.py `
		--build_dir "$ORT_BUILD_DIR" `
		--cmake_generator "Visual Studio 17 2022" `
		--cmake_extra_defines CMAKE_POLICY_VERSION_MINIMUM=3.5 `
		--config $CONFIGURATION `
		--disable_rtti `
		--enable_reduced_operator_type_support `
		--include_ops_by_config "$ROOT_DIR/data/models/required_operators_and_types.with_runtime_opt.config" `
		--parallel `
		--skip_onnx_tests `
		--skip_submodule_sync `
		--skip_tests `
		--use_vcpkg `
		--update `
		--targets $($ORT_COMPONENTS -join ' ') cpuinfo kleidiai `
		--use_dml `
		--use_cuda
}

& python tools/ci_build/build.py `
	--build_dir "$ORT_BUILD_DIR" `
	--cmake_generator "Visual Studio 17 2022" `
	--cmake_extra_defines CMAKE_POLICY_VERSION_MINIMUM=3.5 `
	--config $CONFIGURATION `
	--disable_rtti `
	--enable_reduced_operator_type_support `
	--include_ops_by_config "$ROOT_DIR/data/models/required_operators_and_types.with_runtime_opt.config" `
	--parallel `
	--skip_onnx_tests `
	--skip_submodule_sync `
	--skip_tests `
	--use_vcpkg `
	--build `
	--targets $($ORT_COMPONENTS -join ' ') cpuinfo kleidiai `
	--use_dml `
	--use_cuda

# --- 3. Copy built libraries to .deps_vendor/lib ---
$LIB_DIR = Join-Path $DEPS_DIR "lib"
if (!(Test-Path $LIB_DIR)) { New-Item -ItemType Directory -Path $LIB_DIR | Out-Null }

foreach ($name in $ORT_COMPONENTS) {
	$libPath = Join-Path $ORT_BUILD_DIR "$CONFIGURATION\lib$name.lib"
	if (Test-Path $libPath) {
		Copy-Item $libPath $LIB_DIR
	}
}

$cpuinfoLib = Join-Path $ORT_BUILD_DIR "$CONFIGURATION\_deps\pytorch_cpuinfo-build\cpuinfo.lib"
if (Test-Path $cpuinfoLib) {
	Copy-Item $cpuinfoLib (Join-Path $LIB_DIR "cpuinfo.lib")
}

$kleidiaiLib = Join-Path $ORT_BUILD_DIR "$CONFIGURATION\_deps\kleidiai-build\kleidiai.lib"
if (Test-Path $kleidiaiLib) {
	Copy-Item $kleidiaiLib $LIB_DIR
}
