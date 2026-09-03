#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPOSITORY_ROOT="$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel)"
NCNN_DIR="$REPOSITORY_ROOT/vendor/ncnn"
NCNN_BUILD_DIR="$SCRIPT_DIR/build_ncnn_tools"

cmake \
  -S "$NCNN_DIR" \
  -B "$NCNN_BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DNCNN_BUILD_TOOLS=ON \
  -DNCNN_BUILD_EXAMPLES=OFF \
  -DNCNN_BUILD_BENCHMARK=OFF \
  -DNCNN_BUILD_TESTS=OFF

cmake --build "$NCNN_BUILD_DIR" \
  --config Release \
  --parallel \
  --target ncnn2mem
