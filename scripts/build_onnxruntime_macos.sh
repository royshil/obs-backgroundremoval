#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

# file: scripts/build_onnxruntime_macos.sh
# description: Helper script to build ONNX Runtime for macOS.
# author: Kaito Udagawa <umireon@kaito.tokyo>
# version: 1.2.0
# date: 2026-05-27

set -euo pipefail
shopt -s nullglob

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
: "${CMAKE_BUILD_TYPE:=Release}"
: "${CMAKE_OSX_DEPLOYMENT_TARGET:=12.0}"
: "${PLUGIN_BUILD_DIR:=$ROOT_DIR}"
: "${PYTHON:=python3}"

# shellcheck disable=SC1091
. "$ROOT_DIR/buildspec.props"

: "${onnxruntime_git_tag:=master}"
: "${vcpkg_default_git_tag:=master}"

REDUCED_OPS_CONFIG_NAME='src/required_operators_and_types.with_runtime_opt.config'

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

clone_onnxruntime() {
  if [ -d "$PLUGIN_BUILD_DIR/onnxruntime" ]; then
    git -C "$PLUGIN_BUILD_DIR/onnxruntime" fetch --depth 1 origin "$onnxruntime_git_tag"
    git -C "$PLUGIN_BUILD_DIR/onnxruntime" checkout --force FETCH_HEAD
    git -C "$PLUGIN_BUILD_DIR/onnxruntime" clean -fdx
    git -C "$PLUGIN_BUILD_DIR/onnxruntime" submodule update --init --recursive
  else
    git clone --depth 1 --branch "$onnxruntime_git_tag" --recursive https://github.com/microsoft/onnxruntime.git "$PLUGIN_BUILD_DIR/onnxruntime"
  fi

  patch_onnxruntime
}

patch_onnxruntime() {
  patches=("$ROOT_DIR/scripts/ort_patches/$onnxruntime_git_tag"/*.patch)
  if [[ "${#patches[@]}" -gt 0 ]]; then
    git -C "$PLUGIN_BUILD_DIR/onnxruntime" apply -v "${patches[@]}"
  fi
}

build_py() {
  local -r arch="$1"
  local -r command="$2"

  if ! [[ -d "$PLUGIN_BUILD_DIR/onnxruntime" ]]; then
    echo "ERROR: ONNX Runtime tree is not found." >&2
    exit 1
  fi

  local args=(
    --build_dir "$PLUGIN_BUILD_DIR/build_ort_$arch"
    --osx_arch "$arch"
    --apple_deploy_target "$CMAKE_OSX_DEPLOYMENT_TARGET"
    --cmake_generator Ninja
    --compile_no_warning_as_error
    --config "$CMAKE_BUILD_TYPE"
    --disable_rtti
    --parallel
    --skip_submodule_sync
    --skip_tests
    --use_coreml
    --use_vcpkg
    --cmake_extra_defines
    "CMAKE_POLICY_VERSION_MINIMUM=3.5"
    "onnxruntime_BUILD_UNIT_TESTS=OFF"
  )

  if [[ -f "$ROOT_DIR/$REDUCED_OPS_CONFIG_NAME" ]]; then
    args+=(
      --enable_reduced_operator_type_support
      --include_ops_by_config "$ROOT_DIR/$REDUCED_OPS_CONFIG_NAME"
    )
  fi

  if [[ -n "${CCACHE_DIR:-}" ]]; then
    args+=(--use_cache)
  fi

  "$PYTHON" "$PLUGIN_BUILD_DIR/onnxruntime/tools/ci_build/build.py" "$command" "${args[@]}"
}

install_onnxruntime() {
  local ORT_ARM64_BUILD_DIR="$PLUGIN_BUILD_DIR/build_ort_arm64"
  local ORT_X86_64_BUILD_DIR="$PLUGIN_BUILD_DIR/build_ort_x86_64"
  local ORT_UNIVERSAL_PREFIX="$PLUGIN_BUILD_DIR/ort_installed"
  local ORT_ARM64_PREFIX="$PLUGIN_BUILD_DIR/ort_installed_arm64"
  local ORT_X86_64_PREFIX="$PLUGIN_BUILD_DIR/ort_installed_x86_64"

  rm -rf "$ORT_UNIVERSAL_PREFIX" "$ORT_ARM64_PREFIX" "$ORT_X86_64_PREFIX"

  cmake --install "$ORT_ARM64_BUILD_DIR/$CMAKE_BUILD_TYPE" --config "$CMAKE_BUILD_TYPE" --prefix "$ORT_UNIVERSAL_PREFIX"
  cmake --install "$ORT_ARM64_BUILD_DIR/$CMAKE_BUILD_TYPE" --config "$CMAKE_BUILD_TYPE" --prefix "$ORT_ARM64_PREFIX"
  cmake --install "$ORT_X86_64_BUILD_DIR/$CMAKE_BUILD_TYPE" --config "$CMAKE_BUILD_TYPE" --prefix "$ORT_X86_64_PREFIX"

  local name
  for name in "${ORT_COMPONENTS[@]}"; do
    rm -f "$ORT_UNIVERSAL_PREFIX/lib/lib$name.a"
    lipo -create \
      "$ORT_ARM64_PREFIX/lib/lib$name.a" \
      "$ORT_X86_64_PREFIX/lib/lib$name.a" \
      -output "$ORT_UNIVERSAL_PREFIX/lib/lib$name.a"
  done

  echo 'void __attribute__((visibility("hidden"))) __dummy__(){}' |
    clang -x c -arch x86_64 -c -o "$ORT_X86_64_BUILD_DIR/dummy.o" -mmacosx-version-min="$CMAKE_OSX_DEPLOYMENT_TARGET" -

  libtool -static -o "$ORT_X86_64_BUILD_DIR/dummy.a" "$ORT_X86_64_BUILD_DIR/dummy.o"

  lipo -create \
    "$ORT_ARM64_PREFIX/lib/libkleidiai.a" \
    "$ORT_X86_64_BUILD_DIR/dummy.a" \
    -output "$ORT_UNIVERSAL_PREFIX/lib/libkleidiai.a"

  lipo -create \
    "$ORT_ARM64_BUILD_DIR/$CMAKE_BUILD_TYPE/_deps/pytorch_cpuinfo-build/libcpuinfo.a" \
    "$ORT_X86_64_BUILD_DIR/$CMAKE_BUILD_TYPE/_deps/pytorch_cpuinfo-build/libcpuinfo.a" \
    -output "$ORT_UNIVERSAL_PREFIX/lib/libcpuinfo.a"

  mkdir -p "$ORT_UNIVERSAL_PREFIX/lib/cmake/cpuinfo"
  cat <<'EOF' >"$ORT_UNIVERSAL_PREFIX/lib/cmake/cpuinfo/cpuinfoConfig.cmake"
add_library(cpuinfo::cpuinfo STATIC IMPORTED GLOBAL)
get_filename_component(_CPUINFO_PREFIX "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set_target_properties(cpuinfo::cpuinfo PROPERTIES
  IMPORTED_LOCATION "${_CPUINFO_PREFIX}/lib/libcpuinfo.a"
)
EOF

  "$ROOT_DIR/scripts/lipo_vcpkg_macos.sh" \
    "$PLUGIN_BUILD_DIR/vcpkg_installed_ort/universal-osx" \
    "$ORT_ARM64_BUILD_DIR/$CMAKE_BUILD_TYPE/vcpkg_installed/arm64-osx" \
    "$ORT_X86_64_BUILD_DIR/$CMAKE_BUILD_TYPE/vcpkg_installed/x64-osx"
}

toolchain_report() {
  cd "$ROOT_DIR"

  shasum -a 384 \
    "$REDUCED_OPS_CONFIG_NAME" \
    scripts/build_onnxruntime_macos.sh \
    scripts/lipo_vcpkg_macos.sh \
    scripts/ort_patches/"$onnxruntime_git_tag"/*.patch

  printf 'CMAKE_BUILD_TYPE=%q\n' "$CMAKE_BUILD_TYPE"
  printf 'CMAKE_OSX_DEPLOYMENT_TARGET=%q\n' "$CMAKE_OSX_DEPLOYMENT_TARGET"
  printf 'onnxruntime_git_tag=%q\n' "$onnxruntime_git_tag"
  printf 'REDUCED_OPS_CONFIG_NAME=%q\n' "$REDUCED_OPS_CONFIG_NAME"
  printf 'vcpkg_default_git_tag=%q\n' "$vcpkg_default_git_tag"

  printf 'xcodebuild -version #=> %s\n' "$(xcodebuild -version | /usr/bin/ruby -rjson -e 'print $<.read.to_json')"
  # shellcheck disable=SC2016
  printf '$PYTHON --version #=> %s\n' "$("$PYTHON" --version | /usr/bin/ruby -rjson -e 'print $<.read.to_json')"
  # shellcheck disable=SC2016
  printf '$PYTHON -m pip freeze | sort #=> %s\n' "$("$PYTHON" -m pip freeze | sort | /usr/bin/ruby -rjson -e 'print $<.read.to_json')"
}

if [[ "$#" -eq 0 ]]; then
  clone_onnxruntime
  build_py arm64 --update
  build_py x86_64 --update
  build_py arm64 --build
  build_py x86_64 --build
  install_onnxruntime
else
  "$@"
fi
