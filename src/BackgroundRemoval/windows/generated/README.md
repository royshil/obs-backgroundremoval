<!--
SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>

SPDX-License-Identifier: Apache-2.0
-->

# DirectML generated artifacts

The MediaPipe Landscape DirectML artifacts use the same source as the CoreML generator:
`onnx-community/mediapipe_selfie_segmentation_landscape`, revision
`2497d5bec26c626c7b3c4edc6e1fefc21b64f6c3`, file `onnx/model.onnx` on Hugging Face.
Models under `data/models` belong to the legacy Linux path and are not used here.
Generated artifacts must be verified by a project maintainer and committed to the repository. The C++ file contains
the immutable `BaselineDml` graph for DirectML feature level 1.0 and byte offsets into
the raw float32 weight file. The RC file embeds those weights into the plugin DLL as
`RCDATA`. Fixed weights are operator arguments rather than graph inputs.
`BaselineDmlCompiler` validates the resource bounds and graph edges, orders its nodes,
and converts it into compiled DirectML operators.
Normal builds consume this verified file and do not regenerate it.

The input is contiguous RGB float32 NCHW `[1, 3, 144, 256]` (three planes), and
the output is float32 foreground alpha `[1, 1, 144, 256]`. The generator fixes
the batch size to one, lowers HardSwish to add/clip/multiply, lowers spatial
ReduceMean to average pooling, and folds static Resize shape calculations.

After changing the source model or generator, regenerate the source file:

```console
python -m pip install -r Models/MediaPipeLandscape/DirectML/requirements.txt
python Models/MediaPipeLandscape/DirectML/GenerateModel.py \
  src/BackgroundRemoval/windows/generated/MediaPipeLandscapeDmlProgram.generated.cpp \
  src/BackgroundRemoval/windows/generated/MediaPipeLandscapeDmlProgram.generated.rc \
  src/BackgroundRemoval/windows/generated/MediaPipeLandscapeDmlProgram.weights.bin
```

Add `--offline` to regenerate from the cached pinned model without network access.
Install `Models/MediaPipeLandscape/DirectML/requirements-test.txt`, then run
`python Models/MediaPipeLandscape/DirectML/test_generate_model.py` after downloading
the model to check the input/output contract and numerical equivalence of the lowering.
ONNX Runtime is used only as a CPU test oracle, not by the plugin or its build.

After generation, a project maintainer must inspect and verify the contents.
Commit the verified file in the same change as the generator or source-model
update. Do not edit the generated file manually.
