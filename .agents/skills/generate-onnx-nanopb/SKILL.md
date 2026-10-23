---
# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

name: generate-onnx-nanopb
description: Generate ONNX nanopb test bindings when requested or when the schema or generator changes.
---

Use this skill to generate the ONNX nanopb bindings in `tests/nanopb` when requested or when the ONNX schema or nanopb generator changes. It keeps test serialization code consistent with the pinned inputs and does not cover DirectML model generation.

## Generate the bindings

1. From the repository root, initialize nanopb with `git submodule update --init vendor/nanopb`.
2. In a Python virtual environment (Python 3.12 is recommended), run `python -m pip install -r vendor/nanopb/requirements.txt`.
3. Run `python vendor/nanopb/generator/nanopb_generator.py -I vendor/onnx/onnx -D tests/nanopb vendor/onnx/onnx/onnx.proto`.
4. Preserve the `.license` sidecars and existing line endings of `onnx.pb.c` and `onnx.pb.h`; do not manually edit or clang-format generated code.
