#!/bin/bash

# SPDX-FileCopyrightText: 2025-2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

set -euo pipefail

pretty_cmake() {
	if command -v xcbeautify &>/dev/null && [[ "${RUNNER_DEBUG-}" != 1 ]]; then
		cmake "$@" 2>&1 | xcbeautify
	else
		cmake "$@"
	fi
}

ROOT_DIR="$(cd "$(dirname "$0")"/.. && pwd)"

# ------------------------------------------------------------------------------
# 1. Setup Phase
# ------------------------------------------------------------------------------
echo "::group::Initialize and Check Dependencies"

CMAKE_OSX_ARCHITECTURES="arm64;x86_64"
CMAKE_OSX_DEPLOYMENT_TARGET="12.0"

DEPS_DIR="${ROOT_DIR}/.deps"

if [[ ! -f "${DEPS_DIR}/.deps_versions" ]]; then
	echo "Dependencies not found. Please run \`cmake -P scripts/download_deps.cmake\` first."
	exit 1
fi

. "${DEPS_DIR}/.deps_versions"

PREBUILT_DIR="${DEPS_DIR}/obs-deps-${PREBUILT_VERSION}-universal"
QT6_DIR="${DEPS_DIR}/obs-deps-qt6-${QT6_VERSION}-universal"

SOURCE_DIR="${DEPS_DIR}/obs-studio-${OBS_VERSION}"
BUILD_DIR="${SOURCE_DIR}/build_universal"

if [[ ! -d $SOURCE_DIR ]]; then
	echo "Error: OBS source directory not found at $SOURCE_DIR"
	exit 1
fi

echo "Cleaning build directory..."
rm -rf "$BUILD_DIR"

echo "::endgroup::"

# ------------------------------------------------------------------------------
# 2. Configure Phase
# ------------------------------------------------------------------------------
echo "::group::Configure OBS Studio (Universal)"

cmake -S "$SOURCE_DIR" \
	-B "$BUILD_DIR" \
	-G "Xcode" \
	-DCMAKE_OSX_ARCHITECTURES="${CMAKE_OSX_ARCHITECTURES}" \
	-DCMAKE_OSX_DEPLOYMENT_TARGET="${CMAKE_OSX_DEPLOYMENT_TARGET}" \
	-DOBS_CMAKE_VERSION=3.0.0 \
	-DENABLE_PLUGINS=OFF \
	-DENABLE_FRONTEND=OFF \
	-DOBS_VERSION_OVERRIDE="$OBS_VERSION" \
	"-DCMAKE_PREFIX_PATH=${PREBUILT_DIR};${QT6_DIR}" \
	"-DCMAKE_INSTALL_PREFIX=$DEPS_DIR"

echo "::endgroup::"

# ------------------------------------------------------------------------------
# 3. Build Phase (Debug)
# ------------------------------------------------------------------------------
echo "::group::Build OBS Frontend API (Debug)"

pretty_cmake --build "$BUILD_DIR" \
	--target obs-frontend-api \
	--config Debug \
	--parallel

echo "::endgroup::"

# ------------------------------------------------------------------------------
# 4. Build Phase (Release)
# ------------------------------------------------------------------------------
echo "::group::Build OBS Frontend API (Release)"

pretty_cmake --build "$BUILD_DIR" \
	--target obs-frontend-api \
	--config Release \
	--parallel

echo "::endgroup::"

# ------------------------------------------------------------------------------
# 5. Install Phase (Debug)
# ------------------------------------------------------------------------------
echo "::group::Install Development Artifacts (Debug)"

cmake --install "$BUILD_DIR" \
	--component Development \
	--config Debug \
	--prefix "$DEPS_DIR"

echo "::endgroup::"

# ------------------------------------------------------------------------------
# 6. Install Phase (Release)
# ------------------------------------------------------------------------------
echo "::group::Install Development Artifacts (Release)"

cmake --install "$BUILD_DIR" \
	--component Development \
	--config Release \
	--prefix "$DEPS_DIR"

find "$DEPS_DIR" -type f \( -name "*.prl" -o -name "*.cmake" -o -name "*.conf" -o -name "*.pri" \) -print0 | while IFS= read -r -d '' file; do
	sed -i '' \
		-e 's/-framework AGL//g' \
		-e 's/-framework;AGL//g' \
		-e 's/set(__opengl_agl_fw_path "${WrapOpenGL_AGL}")//g' \
		"$file"
done

echo "Install done. Artifacts are in $DEPS_DIR"
echo "::endgroup::"
