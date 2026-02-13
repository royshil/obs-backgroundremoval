#!/bin/bash
set -euo pipefail

ORT_VERSION="v1.24.1"
CONFIGURATION="RelWithDebInfo"
ORT_COMPONENTS=(onnxruntime_session onnxruntime_optimizer onnxruntime_providers onnxruntime_lora onnxruntime_framework onnxruntime_graph onnxruntime_util onnxruntime_mlas onnxruntime_common onnxruntime_flatbuffers)

cd "$(dirname "${BASH_SOURCE[0]}")/.."

ROOT_DIR="$(pwd)"

mkdir -p .deps_vendor

cd .deps_vendor

# --- 1. Clone ONNX Runtime repository ---

if [[ -d onnxruntime ]]; then
	cd onnxruntime
else
	git clone --depth 1 --branch "$ORT_VERSION" https://github.com/microsoft/onnxruntime.git
	cd onnxruntime
	git submodule update --init --recursive --depth 1
fi

# --- 2. Build ONNX Runtime for macOS ARM64 ---

if ! [[ -d $ROOT_DIR/.deps_vendor/ort_arm64 ]]; then
	python3 tools/ci_build/build.py \
		--build_dir "$ROOT_DIR/.deps_vendor/ort_arm64" \
		--config "$CONFIGURATION" \
		--use_xcode \
		--use_vcpkg \
		--update \
		--osx_arch arm64 \
		--apple_deploy_target 12.0 \
		--cmake_extra_defines CMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
		--disable_rtti \
		--include_ops_by_config "$ROOT_DIR/scripts/required_operators.config" \
		--compile_no_warning_as_error
fi

python3 tools/ci_build/build.py \
	--build_dir "$ROOT_DIR/.deps_vendor/ort_arm64" \
	--config "$CONFIGURATION" \
	--use_xcode \
	--use_vcpkg \
	--build \
	--skip_tests \
	--parallel \
	--targets "${ORT_COMPONENTS[@]}"

# --- 3. Build ONNX Runtime for macOS x86_64 ---

if ! [[ -d $ROOT_DIR/.deps_vendor/ort_x86_64 ]]; then
	python3 tools/ci_build/build.py \
		--build_dir "$ROOT_DIR/.deps_vendor/ort_x86_64" \
		--config "$CONFIGURATION" \
		--use_xcode \
		--use_vcpkg \
		--update \
		--osx_arch x86_64 \
		--apple_deploy_target 12.0 \
		--cmake_extra_defines CMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
		--disable_rtti \
		--include_ops_by_config "$ROOT_DIR/scripts/required_operators.config" \
		--compile_no_warning_as_error
fi

python3 tools/ci_build/build.py \
	--build_dir "$ROOT_DIR/.deps_vendor/ort_x86_64" \
	--config "$CONFIGURATION" \
	--use_xcode \
	--use_vcpkg \
	--build \
	--skip_tests \
	--parallel \
	--targets "${ORT_COMPONENTS[@]}"

# --- 4. Merge vcpkg_installed into universal ---

bash "$ROOT_DIR/scripts/merge_vcpkg_installed_into_macos_universal.sh" \
	"$ROOT_DIR/.deps_vendor/ort_arm64/$CONFIGURATION/vcpkg_installed/arm64-osx" \
	"$ROOT_DIR/.deps_vendor/ort_x86_64/$CONFIGURATION/vcpkg_installed/x64-osx" \
	"$ROOT_DIR/.deps_vendor/ort_vcpkg_installed/universal-osx"

# --- 5. Create universal libraries ---

mkdir -p "$ROOT_DIR/.deps_vendor/lib"

for name in "${ORT_COMPONENTS[@]}"; do
	lipo -create \
		"$ROOT_DIR/.deps_vendor/ort_arm64/$CONFIGURATION/$CONFIGURATION/lib$name.a" \
		"$ROOT_DIR/.deps_vendor/ort_x86_64/$CONFIGURATION/$CONFIGURATION/lib$name.a" \
		-output "$ROOT_DIR/.deps_vendor/lib/lib$name.a"
done
