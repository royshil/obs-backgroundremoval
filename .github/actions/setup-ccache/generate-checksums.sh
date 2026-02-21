#!/bin/bash
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
