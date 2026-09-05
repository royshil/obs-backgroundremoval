#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

"""Convert the MediaPipe Selfie Segmenter landscape ONNX model to Core ML.

Source: https://huggingface.co/onnx-community/mediapipe_selfie_segmentation_landscape
Contract: pixel_values RGB Float16 [1, 3, 144, 256] -> alphas Float16 [1, 1, 144, 256]
"""

import shutil
from pathlib import Path

import coremltools as ct
from coremltools.converters.mil.mil import types
from huggingface_hub import hf_hub_download
import onnx
import torch
from onnx2torch import convert

IMAGE_SHAPE = (1, 3, 144, 256)
MODEL_REPOSITORY = "onnx-community/mediapipe_selfie_segmentation_landscape"
MODEL_REVISION = "2497d5bec26c626c7b3c4edc6e1fefc21b64f6c3"
MODEL_FILENAME = "onnx/model.onnx"
OUTPUT_PATH = (
    Path(__file__).resolve().parents[3]
    / "src/BackgroundRemoval/macos/generated/MediaPipeLandscape.mlpackage"
)


OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
temporary_output_path = OUTPUT_PATH.with_name("MediaPipeLandscape.generated.mlpackage")
if temporary_output_path.is_dir():
    shutil.rmtree(temporary_output_path)
elif temporary_output_path.exists():
    temporary_output_path.unlink()

onnx_model_path = Path(
    hf_hub_download(
        repo_id=MODEL_REPOSITORY,
        revision=MODEL_REVISION,
        filename=MODEL_FILENAME,
    )
)
if MODEL_REVISION not in onnx_model_path.parts:
    raise RuntimeError(f"Expected model revision {MODEL_REVISION}, got {onnx_model_path}")

onnx_model = onnx.load(onnx_model_path)
onnx.checker.check_model(onnx_model)

segmenter = convert(onnx_model)
segmenter.eval()

torch.manual_seed(0)
example_image = torch.rand(*IMAGE_SHAPE)

with torch.no_grad():
    traced_model = torch.jit.trace(segmenter, example_image)

mlpackage_model = ct.convert(
    traced_model,
    source="pytorch",
    inputs=[
        ct.TensorType(
            name="pixel_values",
            shape=IMAGE_SHAPE,
            dtype=types.fp16,
        ),
    ],
    outputs=[
        ct.TensorType(
            name="alphas",
            dtype=types.fp16,
        )
    ],
    convert_to="mlprogram",
    compute_precision=ct.precision.FLOAT16,
    minimum_deployment_target=ct.target.macOS13,
)
mlpackage_model.short_description = (
    "Based on MediaPipe Selfie Segmentation from "
    "https://huggingface.co/onnx-community/mediapipe_selfie_segmentation_landscape"
)
mlpackage_model.author = "Kaito Udagawa"
mlpackage_model.license = "Apache-2.0"
mlpackage_model.version = MODEL_REVISION[:7]
mlpackage_model.input_description["pixel_values"] = "RGB image in NCHW layout"
mlpackage_model.output_description["alphas"] = "Coarse mask"

mlpackage_model.save(temporary_output_path)
if OUTPUT_PATH.is_dir():
    shutil.rmtree(OUTPUT_PATH)
elif OUTPUT_PATH.exists():
    OUTPUT_PATH.unlink()
temporary_output_path.rename(OUTPUT_PATH)
print(f"saved {OUTPUT_PATH}")
