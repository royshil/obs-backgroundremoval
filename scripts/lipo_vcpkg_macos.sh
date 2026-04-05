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
    lib_rel_path="${lib_full_path#${VCPKG_INSTALLED_ARM64}/}"

    echo "Processing ${lib_rel_path}..."

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

if [[ "$#" -ne 3 ]]; then
  echo "Usage: $0 <vcpkg_installed_universal> <vcpkg_installed_arm64> <vcpkg_installed_x86_64>"
  exit 1
fi

lipo_vcpkg "$@"
