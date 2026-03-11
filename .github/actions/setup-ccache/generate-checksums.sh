#!/bin/bash

# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

set -euo pipefail

if [[ -z "${1:-}" ]]; then
	echo "Usage: $0 <version>"
	echo "# $0 4.12.3"
	exit 1
fi

VERSION="$1"
OUTPUT_DIR="$PWD"
FILES=(
	"ccache-$VERSION-darwin.tar.gz"
	"ccache-$VERSION-windows-x86_64.zip"
	"ccache-$VERSION-linux-x86_64.tar.xz"
)

WORK_DIR=$(mktemp -d)
trap 'rm -rf "$WORK_DIR"' EXIT

cd "$WORK_DIR"

for FILE in "${FILES[@]}"; do
	curl -fsSLO "https://github.com/ccache/ccache/releases/download/v$VERSION/$FILE"
done

sha256sum "${FILES[@]}" >"$OUTPUT_DIR/SHA256SUMS-$VERSION.txt"

cat <<EOF >"$OUTPUT_DIR/SHA256SUMS-$VERSION.txt.license"
SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>

SPDX-License-Identifier: CC0-1.0
EOF
