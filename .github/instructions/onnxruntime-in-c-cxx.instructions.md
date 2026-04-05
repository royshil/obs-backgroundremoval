---
# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

# file: .github/instructions/onnxruntime-in-c-cxx.instructions.md
# author: Kaito Udagawa <umireon@kaito.tokyo>
# version: 1.0.0
# date: 2026-04-04

applyTo: "**/*.{c,h,cpp,hpp}"
---

# Guidelines for Using ONNX Runtime C/C++ API

## Rules

- **Use flattened header paths**: When including ONNX Runtime headers, use the flattened header paths. For example, use `#include <onnxruntime/onnxruntime_cxx_api.h>` instead of `#include <onnxruntime/core/session/onnxruntime_cxx_api.h>`.
