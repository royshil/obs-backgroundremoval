#!/bin/bash
set -euo pipefail

ORT_VERSION="v1.24.1"
CONFIGURATION="Release"
ORT_COMPONENTS=(onnxruntime_session onnxruntime_optimizer onnxruntime_providers onnxruntime_lora onnxruntime_framework onnxruntime_graph onnxruntime_util onnxruntime_mlas onnxruntime_common onnxruntime_flatbuffers onnxruntime_providers_coreml coreml_proto)

ROOT_DIR="$(pwd)"

mkdir -p "$ROOT_DIR/.deps_vendor"

cd "$ROOT_DIR/.deps_vendor"

# --- 1. Clone ONNX Runtime repository ---

if [[ -d onnxruntime ]]; then
	cd onnxruntime
else
	git clone --depth 1 --branch "$ORT_VERSION" https://github.com/microsoft/onnxruntime.git
	cd onnxruntime
	git submodule update --init --recursive --depth 1
	cp cmake/CMakeLists.txt cmake/CMakeLists.txt.orig
	{
		echo 'macro(install)'
		echo 'endmacro()'
		cat cmake/CMakeLists.txt.orig
	} >cmake/CMakeLists.txt
fi

# --- 2. Build ONNX Runtime for macOS ARM64 ---

[[ -d $ROOT_DIR/.deps_vendor/ort_arm64 ]] || python3 tools/ci_build/build.py \
	--build_dir "$ROOT_DIR/.deps_vendor/ort_arm64" \
	--osx_arch arm64 \
	--targets "${ORT_COMPONENTS[@]}" cpuinfo kleidiai \
	--update \
	--apple_deploy_target 12.0 \
	--cmake_extra_defines \
	CMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
	CMAKE_POLICY_VERSION_MINIMUM=3.5 \
	CMAKE_C_COMPILER_LAUNCHER=ccache \
	CMAKE_CXX_COMPILER_LAUNCHER=ccache \
	--compile_no_warning_as_error \
	--config "$CONFIGURATION" \
	--disable_rtti \
	--enable_reduced_operator_type_support \
	--include_ops_by_config "$ROOT_DIR/data/models/required_operators_and_types.with_runtime_opt.config" \
	--parallel \
	--skip_onnx_tests \
	--skip_submodule_sync \
	--skip_tests \
	--use_coreml \
	--use_vcpkg

python3 tools/ci_build/build.py \
	--build_dir "$ROOT_DIR/.deps_vendor/ort_arm64" \
	--osx_arch arm64 \
	--targets "${ORT_COMPONENTS[@]}" cpuinfo kleidiai \
	--build \
	--apple_deploy_target 12.0 \
	--cmake_extra_defines \
	CMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
	CMAKE_POLICY_VERSION_MINIMUM=3.5 \
	CMAKE_C_COMPILER_LAUNCHER=ccache \
	CMAKE_CXX_COMPILER_LAUNCHER=ccache \
	--compile_no_warning_as_error \
	--config "$CONFIGURATION" \
	--disable_rtti \
	--enable_reduced_operator_type_support \
	--include_ops_by_config "$ROOT_DIR/data/models/required_operators_and_types.with_runtime_opt.config" \
	--parallel \
	--skip_onnx_tests \
	--skip_submodule_sync \
	--skip_tests \
	--use_coreml \
	--use_vcpkg

# --- 3. Build ONNX Runtime for macOS x86_64 ---

[[ -d $ROOT_DIR/.deps_vendor/ort_x86_64 ]] || python3 tools/ci_build/build.py \
	--build_dir "$ROOT_DIR/.deps_vendor/ort_x86_64" \
	--osx_arch x86_64 \
	--targets "${ORT_COMPONENTS[@]}" cpuinfo \
	--update \
	--apple_deploy_target 12.0 \
	--cmake_extra_defines \
	CMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
	CMAKE_POLICY_VERSION_MINIMUM=3.5 \
	CMAKE_C_COMPILER_LAUNCHER=ccache \
	CMAKE_CXX_COMPILER_LAUNCHER=ccache \
	--compile_no_warning_as_error \
	--config "$CONFIGURATION" \
	--disable_rtti \
	--enable_reduced_operator_type_support \
	--include_ops_by_config "$ROOT_DIR/data/models/required_operators_and_types.with_runtime_opt.config" \
	--parallel \
	--skip_onnx_tests \
	--skip_submodule_sync \
	--skip_tests \
	--use_coreml \
	--use_vcpkg

python3 tools/ci_build/build.py \
	--build_dir "$ROOT_DIR/.deps_vendor/ort_x86_64" \
	--osx_arch x86_64 \
	--targets "${ORT_COMPONENTS[@]}" cpuinfo \
	--build \
	--apple_deploy_target 12.0 \
	--cmake_extra_defines \
	CMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
	CMAKE_POLICY_VERSION_MINIMUM=3.5 \
	CMAKE_C_COMPILER_LAUNCHER=ccache \
	CMAKE_CXX_COMPILER_LAUNCHER=ccache \
	--compile_no_warning_as_error \
	--config "$CONFIGURATION" \
	--disable_rtti \
	--enable_reduced_operator_type_support \
	--include_ops_by_config "$ROOT_DIR/data/models/required_operators_and_types.with_runtime_opt.config" \
	--parallel \
	--skip_onnx_tests \
	--skip_submodule_sync \
	--skip_tests \
	--use_coreml \
	--use_vcpkg

# --- 4. Merge vcpkg_installed into universal ---

bash "$ROOT_DIR/scripts/merge_vcpkg_installed_into_macos_universal.sh" \
	"$ROOT_DIR/.deps_vendor/ort_arm64/$CONFIGURATION/vcpkg_installed/arm64-osx" \
	"$ROOT_DIR/.deps_vendor/ort_x86_64/$CONFIGURATION/vcpkg_installed/x64-osx" \
	"$ROOT_DIR/.deps_vendor/ort_vcpkg_installed/universal-osx"

# --- 5. Create universal libraries ---

mkdir -p "$ROOT_DIR/.deps_vendor/lib"

for name in "${ORT_COMPONENTS[@]}"; do
	lipo -create \
		"$ROOT_DIR/.deps_vendor/ort_arm64/$CONFIGURATION/lib$name.a" \
		"$ROOT_DIR/.deps_vendor/ort_x86_64/$CONFIGURATION/lib$name.a" \
		-output "$ROOT_DIR/.deps_vendor/lib/lib$name.a"
done

lipo -create \
	"$ROOT_DIR/.deps_vendor/ort_arm64/$CONFIGURATION/_deps/pytorch_cpuinfo-build/libcpuinfo.a" \
	"$ROOT_DIR/.deps_vendor/ort_x86_64/$CONFIGURATION/_deps/pytorch_cpuinfo-build/libcpuinfo.a" \
	-output "$ROOT_DIR/.deps_vendor/lib/libcpuinfo.a"

cp -a "$ROOT_DIR/.deps_vendor/ort_arm64/$CONFIGURATION/_deps/kleidiai-build/libkleidiai.a" "$ROOT_DIR/.deps_vendor/lib/"
