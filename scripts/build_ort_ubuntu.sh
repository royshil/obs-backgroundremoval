#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

# file: scripts/build_ort_ubuntu.sh
# description: Helper script to build ONNX Runtime for Ubuntu.
# author: Kaito Udagawa <umireon@kaito.tokyo>
# version: 1.1.0
# date: 2026-04-04

set -euo pipefail
shopt -s nullglob

PYTHON="${PYTHON:-python3}"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ORT_SRC_DIR="${ROOT_DIR}/.deps_vendor/onnxruntime"
BUILD_PY="${ORT_SRC_DIR}/tools/ci_build/build.py"
REDUCED_OPS_CONFIG="${ROOT_DIR}/src/required_operators_and_types.with_runtime_opt.config"
ORT_X86_64_BUILD_DIR="${ROOT_DIR}/.deps_vendor/ort_x86_64"
ORT_X86_64_PREFIX="${ROOT_DIR}/.deps_vendor/ort_x86_64-prefix"

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

BUILD_PY_ARGS=(
  --cmake_generator Ninja
  --compile_no_warning_as_error
  --config Release
  --disable_rtti
  --parallel
  --skip_submodule_sync
  --skip_tests
  --use_vcpkg
)

if [[ -f "${REDUCED_OPS_CONFIG}" ]]; then
  BUILD_PY_ARGS+=(
    --enable_reduced_operator_type_support
    --include_ops_by_config "${REDUCED_OPS_CONFIG}"
  )
fi

BUILD_PY_CMAKE_EXTRA_DEFINES=(
  "CMAKE_POLICY_VERSION_MINIMUM=3.5"
  "onnxruntime_BUILD_UNIT_TESTS=OFF"
)

run_build_py() {
  local -r arch="$1"
  local -r command="$2"

  if ! [[ -d "${ORT_SRC_DIR}" ]]; then
    echo "ERROR: ONNX Runtime tree is not found." >&2
    exit 1
  fi

  local commandline=(
    "${PYTHON}"
    "${BUILD_PY}"
    "${BUILD_PY_ARGS[@]}"
    --cmake_extra_defines "${BUILD_PY_CMAKE_EXTRA_DEFINES[@]}"
  )

  case "${arch}" in
  x86_64)
    commandline+=(
      --build_dir "${ORT_X86_64_BUILD_DIR}"
      --targets "${ORT_COMPONENTS[@]}"
    )
    ;;
  *)
    echo "ERROR: Invalid arch ${arch}." >&2
    exit 1
    ;;
  esac

  case "${command}" in
  update)
    commandline+=(--update)
    ;;
  build)
    commandline+=(--build)
    ;;
  *)
    echo "ERROR: Invalid command ${command}." >&2
    exit 1
    ;;
  esac

  if [[ -n "${CCACHE_DIR:-}" ]]; then
    commandline+=(--use_cache)
  fi

  "${commandline[@]}"
}

install_ort() {
  rm -rf "${ORT_X86_64_PREFIX}"
  cmake --install "${ORT_X86_64_BUILD_DIR}/Release" --config Release --prefix "${ORT_X86_64_PREFIX}"
}

if [[ "$#" -eq 0 ]]; then
  run_build_py x86_64 update
  run_build_py x86_64 build
  install_ort
else
  "$@"
fi
