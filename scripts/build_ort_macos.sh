#!/bin/bash

# SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
# SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

ORT_VERSION=v1.24.1
CONFIGURATION=Release
ORT_COMPONENTS=(
	onnxruntime_session
	onnxruntime_optimizer
	onnxruntime_providers
	onnxruntime_lora
	onnxruntime_framework
	onnxruntime_graph
	onnxruntime_util
	onnxruntime_mlas
	onnxruntime_common
	onnxruntime_flatbuffers
	onnxruntime_providers_coreml
	coreml_proto
)
OSX_DEPLOY_TARGET=12.0

PROJECT_ROOT_DIR="$(pwd)"
DEPS_VENDOR_DIR="$PROJECT_ROOT_DIR/.deps_vendor"
ORT_SRC_DIR="$DEPS_VENDOR_DIR/onnxruntime"
BUILD_PY="$ORT_SRC_DIR/tools/ci_build/build.py"
LIB_DIR="$DEPS_VENDOR_DIR/lib"
ORT_ARM64_BUILD_DIR="$DEPS_VENDOR_DIR/ort_arm64"
ORT_X86_64_BUILD_DIR="$DEPS_VENDOR_DIR/ort_x86_64"
ORT_VCPKG_DIR="$DEPS_VENDOR_DIR/ort_vcpkg_installed"

mkdir -p "$DEPS_VENDOR_DIR"

# --- 1. Clone ONNX Runtime repository ---

if ! [[ -d "$ORT_SRC_DIR" ]]; then
	git clone --depth 1 --branch "$ORT_VERSION" https://github.com/microsoft/onnxruntime.git "$ORT_SRC_DIR"
	(cd "$ORT_SRC_DIR" && git submodule update --init --recursive --depth 1)
	cp "$ORT_SRC_DIR/cmake/CMakeLists.txt" "$ORT_SRC_DIR/cmake/CMakeLists.txt.orig"
	{
		echo 'macro(install)'
		echo 'endmacro()'
		cat "$ORT_SRC_DIR/cmake/CMakeLists.txt.orig"
	} >"$ORT_SRC_DIR/cmake/CMakeLists.txt"
fi

# --- 2. Common arguments for building ONNX Runtime for macOS ARM64 and x86_64 ---

commonArgs=(
	"--config" "$CONFIGURATION"
	"--parallel"
	"--compile_no_warning_as_error"
	"--use_cache"
	"--cmake_extra_defines"
	"CMAKE_C_COMPILER_LAUNCHER=ccache"
	"CMAKE_CXX_COMPILER_LAUNCHER=ccache"
	"CMAKE_OSX_DEPLOYMENT_TARGET=$OSX_DEPLOY_TARGET"
	"CMAKE_POLICY_VERSION_MINIMUM=3.5"
	"--use_vcpkg"
	"--skip_submodule_sync"
	"--skip_tests"
	"--include_ops_by_config" "$PROJECT_ROOT_DIR/data/models/required_operators_and_types.with_runtime_opt.config"
	"--enable_reduced_operator_type_support"
	"--disable_rtti"
	"--apple_deploy_target" "$OSX_DEPLOY_TARGET"
	"--use_coreml"
)

# --- 3. Build ONNX Runtime for macOS ARM64 ---

if ! [[ -d "$ORT_ARM64_BUILD_DIR" ]]; then
	python3 "$BUILD_PY" --update --build_dir "$ORT_ARM64_BUILD_DIR" "${commonArgs[@]}" --osx_arch arm64 --targets "${ORT_COMPONENTS[@]}" cpuinfo kleidiai
fi

python3 "$BUILD_PY" --build --build_dir "$ORT_ARM64_BUILD_DIR" "${commonArgs[@]}" --osx_arch arm64 --targets "${ORT_COMPONENTS[@]}" cpuinfo kleidiai

# --- 4. Build ONNX Runtime for macOS x86_64 ---

if ! [[ -d "$ORT_X86_64_BUILD_DIR" ]]; then
	python3 "$BUILD_PY" --update --build_dir "$ORT_X86_64_BUILD_DIR" "${commonArgs[@]}" --osx_arch x86_64 --targets "${ORT_COMPONENTS[@]}" cpuinfo
fi

python3 "$BUILD_PY" --build --build_dir "$ORT_X86_64_BUILD_DIR" "${commonArgs[@]}" --osx_arch x86_64 --targets "${ORT_COMPONENTS[@]}" cpuinfo

# --- 5. Merge vcpkg_installed into universal ---


bash "$PROJECT_ROOT_DIR/scripts/lipo_vcpkg_macos.sh" \
	"$ORT_ARM64_BUILD_DIR/$CONFIGURATION/vcpkg_installed/arm64-osx" \
	"$ORT_X86_64_BUILD_DIR/$CONFIGURATION/vcpkg_installed/x64-osx" \
	"$ORT_VCPKG_DIR/universal-osx"

# --- 6. Create universal libraries ---

mkdir -p "$DEPS_VENDOR_DIR/lib"

for name in "${ORT_COMPONENTS[@]}"; do
	lipo -create \
		"$ORT_ARM64_BUILD_DIR/$CONFIGURATION/lib$name.a" \
		"$ORT_X86_64_BUILD_DIR/$CONFIGURATION/lib$name.a" \
		-output "$LIB_DIR/lib$name.a"
done

lipo -create \
	"$ORT_ARM64_BUILD_DIR/$CONFIGURATION/_deps/pytorch_cpuinfo-build/libcpuinfo.a" \
	"$ORT_X86_64_BUILD_DIR/$CONFIGURATION/_deps/pytorch_cpuinfo-build/libcpuinfo.a" \
	-output "$LIB_DIR/libcpuinfo.a"

echo 'void __attribute__((visibility("hidden"))) __dummy__(){}' |
  clang -x c -arch x86_64 -c -o "$ORT_X86_64_BUILD_DIR/dummy.o" -mmacosx-version-min="$OSX_DEPLOY_TARGET" -

libtool -static -o "$ORT_X86_64_BUILD_DIR/dummy.a" "$ORT_X86_64_BUILD_DIR/dummy.o"

lipo -create \
	"$ORT_ARM64_BUILD_DIR/$CONFIGURATION/_deps/kleidiai-build/libkleidiai.a" \
	"$ORT_X86_64_BUILD_DIR/dummy.a" \
    -output "$LIB_DIR/libkleidiai.a"
