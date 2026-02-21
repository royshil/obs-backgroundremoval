#!/bin/bash
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
)

ROOT_DIR="$(pwd)"
DEPS_DIR="$ROOT_DIR/.deps_vendor"
mkdir -p "$DEPS_DIR"
ORT_SRC_DIR="$DEPS_DIR/onnxruntime"
BUILD_PY="$ORT_SRC_DIR/tools/ci_build/build.py"
ORT_BUILD_DIR="$DEPS_DIR/ort_x86_64"
LIB_DIR="$DEPS_DIR/lib"

# --- 1. Clone ONNX Runtime repository ---

if ! [[ -d $ORT_SRC_DIR ]]; then
	git clone --depth 1 --branch "$ORT_VERSION" https://github.com/microsoft/onnxruntime.git "$ORT_SRC_DIR"
	(cd "$ORT_SRC_DIR" && git submodule update --init --recursive --depth 1)
fi

# --- 2. Build ONNX Runtime for Ubuntu x86_64 ---

commonArgs=(
	"--build_dir" "$ORT_BUILD_DIR"
	"--config" "$CONFIGURATION"
	"--parallel"
	"--compile_no_warning_as_error"
	"--cmake_extra_defines"
	"CMAKE_POLICY_VERSION_MINIMUM=3.5"
	"--use_cache"
	"--use_vcpkg"
	"--skip_submodule_sync"
	"--skip_tests"
	"--include_ops_by_config" "$ROOT_DIR/data/models/required_operators_and_types.with_runtime_opt.config"
	"--enable_reduced_operator_type_support"
	"--disable_rtti"
)

if ! [[ -d $ORT_BUILD_DIR ]]; then
	python3 "$BUILD_PY" --update "${commonArgs[@]}" --targets "${ORT_COMPONENTS[@]}"
fi

python3 "$BUILD_PY" --build "${commonArgs[@]}" --targets "${ORT_COMPONENTS[@]}"

# --- 3. Install ORT libraries ---

mkdir -p "$LIB_DIR"

for name in "${ORT_COMPONENTS[@]}"; do
	cp -a "$ROOT_DIR/.deps_vendor/ort_x86_64/$CONFIGURATION/lib$name.a" "$LIB_DIR/"
done
