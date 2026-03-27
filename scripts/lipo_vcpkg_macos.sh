#!/bin/bash

# SPDX-FileCopyrightText: 2025-2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

set -euo pipefail
shopt -s nullglob

if [[ "$#" -ne 3 ]]; then
  echo "Usage: $0 <vcpkg_installed_arm64> <vcpkg_installed_x86_64> <vcpkg_installed_universal>"
  exit 1
fi

rm -rf "$3"
mkdir -p "$3"

VCPKG_INSTALLED_ARM64="$(cd "$1" && pwd)"
VCPKG_INSTALLED_X86_64="$(cd "$2" && pwd)"
VCPKG_INSTALLED_UNIVERSAL="$(cd "$3" && pwd)"

echo "VCPKG_INSTALLED_ARM64: $VCPKG_INSTALLED_ARM64"
echo "VCPKG_INSTALLED_X86_64: $VCPKG_INSTALLED_X86_64"
echo "VCPKG_INSTALLED_UNIVERSAL: $VCPKG_INSTALLED_UNIVERSAL"

mkdir -p "$VCPKG_INSTALLED_UNIVERSAL/"{debug/lib/pkgconfig,include,lib/pkgconfig,share}

cp -a "$VCPKG_INSTALLED_ARM64/include/" "$VCPKG_INSTALLED_UNIVERSAL/include/"
cp -a "$VCPKG_INSTALLED_ARM64/lib/pkgconfig/" "$VCPKG_INSTALLED_UNIVERSAL/lib/pkgconfig/"
cp -a "$VCPKG_INSTALLED_ARM64/share/" "$VCPKG_INSTALLED_UNIVERSAL/share/"

pushd "$VCPKG_INSTALLED_ARM64/lib"
libs=(*.a)
popd

for name in "${libs[@]}"; do
	echo "Processing lib/$name"
	lipo \
		"$VCPKG_INSTALLED_ARM64/lib/$name" \
		"$VCPKG_INSTALLED_X86_64/lib/$name" \
		-create \
		-output "$VCPKG_INSTALLED_UNIVERSAL/lib/$name"
done

if [[ -d "$VCPKG_INSTALLED_ARM64/debug" ]]; then
	cp -a "$VCPKG_INSTALLED_ARM64/debug/lib/pkgconfig/" "$VCPKG_INSTALLED_UNIVERSAL/debug/lib/pkgconfig/"

  pushd "$VCPKG_INSTALLED_ARM64/debug/lib"
  debug_libs=(*.a)
  popd

	for name in "${debug_libs[@]}"; do
		echo "Processing debug/lib/$name"

		lipo \
			"$VCPKG_INSTALLED_ARM64/debug/lib/$name" \
			"$VCPKG_INSTALLED_X86_64/debug/lib/$name" \
			-create \
			-output "$VCPKG_INSTALLED_UNIVERSAL/debug/lib/$name"
	done
fi

if [[ -d "$VCPKG_INSTALLED_ARM64/tools" ]]; then
	mkdir -p "$VCPKG_INSTALLED_UNIVERSAL/tools"
	cp -a "$VCPKG_INSTALLED_ARM64/tools/" "$VCPKG_INSTALLED_UNIVERSAL/tools/"
fi
