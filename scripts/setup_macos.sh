#!/bin/bash

# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

set -euo pipefail

PROJECT_ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
SCRIPTS_DIR="$PROJECT_ROOT_DIR/scripts"

DEPS_VENDOR_DIR="$PROJECT_ROOT_DIR/.deps_vendor"
VCPKG_ARM64_DIR="$DEPS_VENDOR_DIR/vcpkg_installed_arm64"
VCPKG_X64_DIR="$DEPS_VENDOR_DIR/vcpkg_installed_x64"
VCPKG_DIR="$DEPS_VENDOR_DIR/vcpkg_installed"

cd "$PROJECT_ROOT_DIR"

vcpkg --version
cmake --version

vcpkg install --triplet=arm64-osx-obs --x-install-root="$VCPKG_ARM64_DIR"
vcpkg install --triplet=x64-osx-obs --x-install-root="$VCPKG_X64_DIR"

"$SCRIPTS_DIR/merge_vcpkg_installed_into_macos_universal.sh" \
  "$VCPKG_ARM64_DIR/arm64-osx-obs" \
  "$VCPKG_X64_DIR/x64-osx-obs" \
  "$VCPKG_DIR/universal-osx-obs"

cmake -P "$SCRIPTS_DIR/download_deps.cmake"

"$SCRIPTS_DIR/build_libobs_macos.sh"

"$SCRIPTS_DIR/build_ort_macos.sh"
