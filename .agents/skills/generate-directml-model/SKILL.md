---
# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

name: generate-directml-model
description: Generate the MediaPipe Landscape DirectML model when requested or when its generator or pinned source model changes.
---

Use this skill to generate the production MediaPipe Landscape DirectML C++ model from the pinned ONNX source; it does not cover ONNX nanopb bindings.

1. From the repository root, use a Python virtual environment (Python 3.12 is recommended) and put `clang-format` on PATH.
2. Run `python -m pip install -r Models/MediaPipeLandscape/DirectML/requirements.txt`.
3. Run `python Models/MediaPipeLandscape/DirectML/GenerateModel.py src/BackgroundRemoval/windows/generated/MediaPipeLandscapeDmlProgram.generated.cpp` with default production options; add `--offline` to require the cached model.
4. Preserve the pinned revision and SHA-256 check. The generator applies the generated directory's clang-format configuration; do not hand-edit the generated C++.
5. For requested numerical validation on a configured Windows build, build `LandscapeModelCheck` with `cmake --build build --config RelWithDebInfo --target LandscapeModelCheck`, then run `python Models/MediaPipeLandscape/DirectML/VerifyModel.py --offline --runner build/tests/DmlProgram/RelWithDebInfo/LandscapeModelCheck.exe --output-dir build/model-generation-check/numeric`.
