#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

# file: scripts/build_ort_macos.sh
# description: Helper script to build ONNX Runtime for macOS.
# author: Kaito Udagawa <umireon@kaito.tokyo>
# version: 1.1.0
# date: 2026-04-04

set -euo pipefail
shopt -s nullglob

PYTHON="${PYTHON:-python3}"
OSX_DEPLOY_TARGET="${OSX_DEPLOY_TARGET:-12.0}"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ORT_SRC_DIR="${ROOT_DIR}/.deps_vendor/onnxruntime"
BUILD_PY="${ORT_SRC_DIR}/tools/ci_build/build.py"
REDUCED_OPS_CONFIG="${ROOT_DIR}/src/required_operators_and_types.with_runtime_opt.config"
ORT_ARM64_BUILD_DIR="${ROOT_DIR}/.deps_vendor/ort_arm64"
ORT_ARM64_PREFIX="${ROOT_DIR}/.deps_vendor/ort_arm64-prefix"
ORT_X86_64_BUILD_DIR="${ROOT_DIR}/.deps_vendor/ort_x86_64"
ORT_X86_64_PREFIX="${ROOT_DIR}/.deps_vendor/ort_x86_64-prefix"
ORT_UNIVERSAL_PREFIX="${ROOT_DIR}/.deps_vendor/ort_universal-prefix"
ORT_UNIVERSAL_VCPKG_INSTALLED_DIR="${ROOT_DIR}/.deps_vendor/ort_universal_vcpkg_installed/universal-osx"

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

BUILD_PY_ARGS=(
  --apple_deploy_target "${OSX_DEPLOY_TARGET}"
  --cmake_generator Ninja
  --compile_no_warning_as_error
  --config Release
  --disable_rtti
  --parallel
  --skip_submodule_sync
  --skip_tests
  --use_coreml
  --use_vcpkg
)

if [[ -f "${REDUCED_OPS_CONFIG}" ]]; then
  BUILD_PY_ARGS+=(
    --enable_reduced_operator_type_support
    --include_ops_by_config "${REDUCED_OPS_CONFIG}"
  )
fi

BUILD_PY_CMAKE_EXTRA_DEFINES=(
  "CMAKE_OSX_DEPLOYMENT_TARGET=${OSX_DEPLOY_TARGET}"
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
  arm64)
    commandline+=(
      --build_dir "${ORT_ARM64_BUILD_DIR}"
      --osx_arch arm64
      --targets "${ORT_COMPONENTS[@]}" cpuinfo kleidiai
    )
    ;;
  x86_64)
    commandline+=(
      --build_dir "${ORT_X86_64_BUILD_DIR}"
      --osx_arch x86_64
      --targets "${ORT_COMPONENTS[@]}" cpuinfo
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

lipo_vcpkg() {
  local -r VCPKG_INSTALLED_UNIVERSAL="$1"
  local -r VCPKG_INSTALLED_ARM64="$2"
  local -r VCPKG_INSTALLED_X64="$3"

  rm -rf "${VCPKG_INSTALLED_UNIVERSAL}"
  mkdir -p "${VCPKG_INSTALLED_UNIVERSAL}"/{debug/lib/pkgconfig,include,lib/pkgconfig,share}

  cp -a "${VCPKG_INSTALLED_ARM64}/include/." "${VCPKG_INSTALLED_UNIVERSAL}/include/"
  cp -a "${VCPKG_INSTALLED_ARM64}/lib/pkgconfig/." "${VCPKG_INSTALLED_UNIVERSAL}/lib/pkgconfig/"
  cp -a "${VCPKG_INSTALLED_ARM64}/share/." "${VCPKG_INSTALLED_UNIVERSAL}/share/"

  if [[ -d "${VCPKG_INSTALLED_ARM64}/debug" ]]; then
    cp -a "${VCPKG_INSTALLED_ARM64}/debug/lib/pkgconfig/." "${VCPKG_INSTALLED_UNIVERSAL}/debug/lib/pkgconfig/"
  fi

  if [[ -d "${VCPKG_INSTALLED_ARM64}/tools" ]]; then
    mkdir -p "${VCPKG_INSTALLED_UNIVERSAL}/tools"
    cp -a "${VCPKG_INSTALLED_ARM64}/tools/." "${VCPKG_INSTALLED_UNIVERSAL}/tools/"
  fi

  local lib_full_path lib_rel_path arm64_path x64_path universal_path
  for lib_full_path in "${VCPKG_INSTALLED_ARM64}"/lib/*.a "${VCPKG_INSTALLED_ARM64}"/debug/lib/*.a; do
    lib_rel_path="${lib_full_path#"${VCPKG_INSTALLED_ARM64}"/}"

    arm64_path="${VCPKG_INSTALLED_ARM64}/${lib_rel_path}"
    x64_path="${VCPKG_INSTALLED_X64}/${lib_rel_path}"
    universal_path="${VCPKG_INSTALLED_UNIVERSAL}/${lib_rel_path}"

    if ! [[ -f "${x64_path}" ]]; then
      echo "ERROR: ${x64_path} does not exist." >&2
      exit 1
    fi

    lipo "${arm64_path}" "${x64_path}" -create -output "${universal_path}"
  done
}

install_ort() {
  rm -rf "${ORT_UNIVERSAL_PREFIX}" "${ORT_ARM64_PREFIX}" "${ORT_X86_64_PREFIX}"

  cmake --install "${ORT_ARM64_BUILD_DIR}/Release" --config Release --prefix "${ORT_UNIVERSAL_PREFIX}"
  cmake --install "${ORT_ARM64_BUILD_DIR}/Release" --config Release --prefix "${ORT_ARM64_PREFIX}"
  cmake --install "${ORT_X86_64_BUILD_DIR}/Release" --config Release --prefix "${ORT_X86_64_PREFIX}"

  local name
  for name in "${ORT_COMPONENTS[@]}"; do
    rm -f "${ORT_UNIVERSAL_PREFIX}/lib/lib${name}.a"
    lipo -create \
      "${ORT_ARM64_PREFIX}/lib/lib${name}.a" \
      "${ORT_X86_64_PREFIX}/lib/lib${name}.a" \
      -output "${ORT_UNIVERSAL_PREFIX}/lib/lib${name}.a"
  done

  echo 'void __attribute__((visibility("hidden"))) __dummy__(){}' |
    clang -x c -arch x86_64 -c -o "${ORT_X86_64_BUILD_DIR}/dummy.o" -mmacosx-version-min="${OSX_DEPLOY_TARGET}" -

  libtool -static -o "${ORT_X86_64_BUILD_DIR}/dummy.a" "${ORT_X86_64_BUILD_DIR}/dummy.o"

  lipo -create \
    "${ORT_ARM64_PREFIX}/lib/libkleidiai.a" \
    "${ORT_X86_64_BUILD_DIR}/dummy.a" \
    -output "${ORT_UNIVERSAL_PREFIX}/lib/libkleidiai.a"

  lipo -create \
    "${ORT_ARM64_BUILD_DIR}/Release/_deps/pytorch_cpuinfo-build/libcpuinfo.a" \
    "${ORT_X86_64_BUILD_DIR}/Release/_deps/pytorch_cpuinfo-build/libcpuinfo.a" \
    -output "${ORT_UNIVERSAL_PREFIX}/lib/libcpuinfo.a"

  mkdir -p "${ORT_UNIVERSAL_PREFIX}/lib/cmake/cpuinfo"
  cat <<'EOF' >"${ORT_UNIVERSAL_PREFIX}/lib/cmake/cpuinfo/cpuinfoConfig.cmake"
add_library(cpuinfo::cpuinfo STATIC IMPORTED GLOBAL)
get_filename_component(_CPUINFO_PREFIX "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set_target_properties(cpuinfo::cpuinfo PROPERTIES
  IMPORTED_LOCATION "${_CPUINFO_PREFIX}/lib/libcpuinfo.a"
)
EOF

  lipo_vcpkg \
    "${ORT_UNIVERSAL_VCPKG_INSTALLED_DIR}" \
    "${ORT_ARM64_BUILD_DIR}/Release/vcpkg_installed/arm64-osx" \
    "${ORT_X86_64_BUILD_DIR}/Release/vcpkg_installed/x64-osx"
}

if [[ "$#" -eq 0 ]]; then
  run_build_py arm64 update
  run_build_py x86_64 update
  run_build_py arm64 build
  run_build_py x86_64 build
  install_ort
else
  "$@"
fi
