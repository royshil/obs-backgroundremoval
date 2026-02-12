#!/bin/bash
set -euo pipefail

ORT_VERSION="v1.24.1"

cd "$(dirname "${BASH_SOURCE[0]}" )"/..

ROOT_DIR="$(pwd)"

mkdir -p build_macos

cd build_macos

if [[ -d onnxruntime ]]; then
  cd onnxruntime
else
  git clone --depth 1 --branch "$ORT_VERSION" https://github.com/microsoft/onnxruntime.git
  cd onnxruntime
  git submodule update --init --recursive --depth 1
fi

./build.sh \
  --config RelWithDebInfo \
  --update \
  --build \
  --parallel \
  --skip_tests \
  --osx_arch arm64 \
  --apple_deploy_target 12.0 \
  --disable_rtti \
  --include_ops_by_config "$ROOT_DIR/scripts/required_operators.config"
