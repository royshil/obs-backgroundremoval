<!--
SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>

SPDX-License-Identifier: Apache-2.0
-->

# MediaPipe Landscape Shader

## Attribution

Based on Google's MediaPipe Selfie Segmentation landscape model, distributed as
[ONNX by onnx-community](https://huggingface.co/onnx-community/mediapipe_selfie_segmentation_landscape/tree/2497d5bec26c626c7b3c4edc6e1fefc21b64f6c3).
Generated weights: Copyright 2021 Google LLC and 2026 Kaito Udagawa; Apache-2.0.
Conversion code: Copyright 2026 Kaito Udagawa; Apache-2.0.

## Foundation

The network is written by hand as OBS pixel shaders in
`src/BackgroundRemoval/MediaPipeLandscapeShader/MPLSEffect.cpp`, one fixed-shape
technique per layer, and wired pass by pass in
`src/BackgroundRemoval/MediaPipeLandscapeShader/MPLSPipeline.cpp`.
`GenerateWeights.py` only packs the weights of each layer, exact in IEEE half, into its own
RGBA16F weight and bias textures and writes
`src/BackgroundRemoval/MediaPipeLandscapeShader/generated/MPLSPipeline.generated.cpp`,
formatted with the clang-format 21 that `requirements.txt` pins.

```sh
python -m pip install -r Models/MediaPipeLandscape/Shader/requirements.txt
python Models/MediaPipeLandscape/Shader/GenerateWeights.py
```

## Target system

Any system where OBS renders with Direct3D 11 or OpenGL. No inference runtime
and no CPU readback are involved.
