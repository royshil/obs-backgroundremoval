#!/bin/bash

# SPDX-FileCopyrightText: 2025-2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

# file: scripts/lipo_vcpkg_macos.sh
# description: Combines vcpkg_installed directories into a universal one.
# author: Kaito Udagawa <umireon@kaito.tokyo>
# version: 1.0.1
# date: 2026-04-02

set -euo pipefail
shopt -s nullglob

lipo_vcpkg() {
  local -r VCPKG_INSTALLED="$1"
  local -r VCPKG_ARM64_INSTALLED="$2"
  local -r VCPKG_X64_INSTALLED="$3"

  rm -rf "$VCPKG_INSTALLED"
  mkdir -p "$VCPKG_INSTALLED"/{debug/lib/pkgconfig,include,lib/pkgconfig,share}

  cp -a "$VCPKG_ARM64_INSTALLED/include/." "$VCPKG_INSTALLED/include/"
  cp -a "$VCPKG_ARM64_INSTALLED/lib/pkgconfig/." "$VCPKG_INSTALLED/lib/pkgconfig/"
  cp -a "$VCPKG_ARM64_INSTALLED/share/." "$VCPKG_INSTALLED/share/"

  if [[ -d "$VCPKG_ARM64_INSTALLED/debug" ]]; then
    cp -a "$VCPKG_ARM64_INSTALLED/debug/lib/pkgconfig/." "$VCPKG_INSTALLED/debug/lib/pkgconfig/"
  fi

  if [[ -d "$VCPKG_ARM64_INSTALLED/tools" ]]; then
    mkdir -p "$VCPKG_INSTALLED/tools"
    cp -a "$VCPKG_ARM64_INSTALLED/tools/." "$VCPKG_INSTALLED/tools/"
  fi

  local lib_full_path lib_rel_path arm64_path x64_path universal_path
  for lib_full_path in "$VCPKG_ARM64_INSTALLED"/lib/*.a "$VCPKG_ARM64_INSTALLED"/debug/lib/*.a; do
    lib_rel_path="${lib_full_path#$VCPKG_ARM64_INSTALLED/}"

    echo "Processing ${lib_rel_path}..."

    arm64_path="$VCPKG_ARM64_INSTALLED/$lib_rel_path"
    x64_path="$VCPKG_X64_INSTALLED/$lib_rel_path"
    universal_path="$VCPKG_INSTALLED/$lib_rel_path"

    if ! [[ -f "$x64_path" ]]; then
      echo "ERROR: $x64_path does not exist." >&2
      exit 1
    fi

    lipo "$arm64_path" "$x64_path" -create -output "$universal_path"
  done
}

if [[ "$#" -ne 3 ]]; then
  echo "Usage: $0 <vcpkg_installed> <vcpkg_arm64_installed> <vcpkg_x64_installed>"
  exit 1
fi

lipo_vcpkg "$@"
