#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

# file: scripts/setup_macos.sh
# description: Convenience script to set up the development environment for macOS.
# author: Kaito Udagawa <umireon@kaito.tokyo>
# version: 1.0.1
# date: 2026-04-02

set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."

if [[ -d .deps_vendor ]]; then
  echo 'The .deps_vendor directory already exists. Exiting...' >&2
  exit 1
fi

echo '## Checking if required tools can be called without errors'

if [[ -z "${VCPKG_ROOT:-}" ]]; then
  echo 'ERROR: $VCPKG_ROOT is not set' >&2
  exit 1
fi

set -x
cmake --version
ninja --version
pkg-config --version
python3 --version
python3 -m pip show flatbuffers
"${VCPKG_ROOT}/vcpkg" --version
set +x

echo '## Installing dependencies'

export VCPKG_BINARY_SOURCES
if [[ -z "${VCPKG_BINARY_SOURCES:-}" ]]; then
  VCPKG_BINARY_SOURCES='clear;http,https://vcpkg-obs.kaito.tokyo/{name}/{version}/{sha}'
fi

"${VCPKG_ROOT}/vcpkg" install --triplet=arm64-osx-obs --x-install-root=.deps_vendor/vcpkg_installed_arm64
"${VCPKG_ROOT}/vcpkg" install --triplet=x64-osx-obs --x-install-root=.deps_vendor/vcpkg_installed_x64

./scripts/lipo_vcpkg_macos.sh \
  .deps_vendor/vcpkg_installed_universal/universal-osx-obs \
  .deps_vendor/vcpkg_installed_arm64/arm64-osx-obs \
  .deps_vendor/vcpkg_installed_x64/x64-osx-obs

cmake -P scripts/download_deps.cmake

./scripts/build_libobs_macos.sh

git clone --depth=1 --branch=v1.24.4 --recursive https://github.com/microsoft/onnxruntime.git .deps_vendor/onnxruntime

patches=(scripts/ort_patches/*.patch)
if [[ "${#patches[@]}" -gt 0 ]]; then
  git -C .deps_vendor/onnxruntime am --no-gpg-sign --3way "../../${patches[@]}"
fi

./scripts/build_ort_macos.sh run_build_py arm64 update
./scripts/build_ort_macos.sh run_build_py x86_64 update

if command -v xcbeautify >/dev/null 2>&1; then
  ./scripts/build_ort_macos.sh run_build_py arm64 build | xcbeautify
  ./scripts/build_ort_macos.sh run_build_py x86_64 build | xcbeautify
else
  echo 'Running ort build without xcbeautify since it is not available.' >&2
  ./scripts/build_ort_macos.sh run_build_py arm64 build
  ./scripts/build_ort_macos.sh run_build_py x86_64 build
fi

./scripts/build_ort_macos.sh install_ort
