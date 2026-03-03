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

ROOT_DIR="$(pwd)"
DEPS_DIR="$ROOT_DIR/.deps_vendor"
mkdir -p "$DEPS_DIR"
ORT_SRC_DIR="$DEPS_DIR/onnxruntime"
BUILD_PY="$ORT_SRC_DIR/tools/ci_build/build.py"
LIB_DIR="$DEPS_DIR/lib"

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
	"--include_ops_by_config" "$ROOT_DIR/data/models/required_operators_and_types.with_runtime_opt.config"
	"--enable_reduced_operator_type_support"
	"--disable_rtti"
	"--apple_deploy_target" "$OSX_DEPLOY_TARGET"
	"--use_coreml"
)

# --- 3. Build ONNX Runtime for macOS ARM64 ---

ORT_ARM64_BUILD_DIR="$ROOT_DIR/.deps_vendor/ort_arm64"

if ! [[ -d "$ORT_ARM64_BUILD_DIR" ]]; then
	python3 "$BUILD_PY" --update --build_dir "$ORT_ARM64_BUILD_DIR" "${commonArgs[@]}" --osx_arch arm64 --targets "${ORT_COMPONENTS[@]}" cpuinfo kleidiai
fi

python3 "$BUILD_PY" --build --build_dir "$ORT_ARM64_BUILD_DIR" "${commonArgs[@]}" --osx_arch arm64 --targets "${ORT_COMPONENTS[@]}" cpuinfo kleidiai

# --- 4. Build ONNX Runtime for macOS x86_64 ---

ORT_X86_64_BUILD_DIR="$ROOT_DIR/.deps_vendor/ort_x86_64"

if ! [[ -d "$ORT_X86_64_BUILD_DIR" ]]; then
	python3 "$BUILD_PY" --update --build_dir "$ORT_X86_64_BUILD_DIR" "${commonArgs[@]}" --osx_arch x86_64 --targets "${ORT_COMPONENTS[@]}" cpuinfo
fi

python3 "$BUILD_PY" --build --build_dir "$ORT_X86_64_BUILD_DIR" "${commonArgs[@]}" --osx_arch x86_64 --targets "${ORT_COMPONENTS[@]}" cpuinfo

# --- 5. Merge vcpkg_installed into universal ---

bash "$ROOT_DIR/scripts/merge_vcpkg_installed_into_macos_universal.sh" \
	"$ROOT_DIR/.deps_vendor/ort_arm64/$CONFIGURATION/vcpkg_installed/arm64-osx" \
	"$ROOT_DIR/.deps_vendor/ort_x86_64/$CONFIGURATION/vcpkg_installed/x64-osx" \
	"$ROOT_DIR/.deps_vendor/ort_vcpkg_installed/universal-osx"

# --- 6. Create universal libraries ---

mkdir -p "$ROOT_DIR/.deps_vendor/lib"

for name in "${ORT_COMPONENTS[@]}"; do
	lipo -create \
		"$ROOT_DIR/.deps_vendor/ort_arm64/$CONFIGURATION/lib$name.a" \
		"$ROOT_DIR/.deps_vendor/ort_x86_64/$CONFIGURATION/lib$name.a" \
		-output "$LIB_DIR/lib$name.a"
done

lipo -create \
	"$ROOT_DIR/.deps_vendor/ort_arm64/$CONFIGURATION/_deps/pytorch_cpuinfo-build/libcpuinfo.a" \
	"$ROOT_DIR/.deps_vendor/ort_x86_64/$CONFIGURATION/_deps/pytorch_cpuinfo-build/libcpuinfo.a" \
	-output "$LIB_DIR/libcpuinfo.a"

cp -a "$ROOT_DIR/.deps_vendor/ort_arm64/$CONFIGURATION/_deps/kleidiai-build/libkleidiai.a" "$LIB_DIR/"
