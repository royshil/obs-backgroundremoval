<!--
SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>

SPDX-License-Identifier: Apache-2.0
-->

# MediaPipe Landscape DirectML

## Attribution

Based on Google's MediaPipe Selfie Segmentation landscape model, distributed as
[ONNX by onnx-community](https://huggingface.co/onnx-community/mediapipe_selfie_segmentation_landscape/tree/2497d5bec26c626c7b3c4edc6e1fefc21b64f6c3).
Generated model: Copyright 2021 Google LLC and 2026 Kaito Udagawa; Apache-2.0.
Conversion code: Copyright 2026 Kaito Udagawa; Apache-2.0.

## Foundation

ONNX graph conversion to immutable C++ graph definitions and embedded FP32 weights,
executed by BaselineDml using DirectML 1.0 and Direct3D 12.
Restricting the model to DirectML 1.0 features limits optimization opportunities
and can reduce performance compared with implementations using newer DirectML features.

## Target system

Windows x64 with a Direct3D 12 / DirectML-capable GPU, integrated into OBS through
the Windows Direct3D 11 pipeline.

