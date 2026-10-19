#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2025-2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

"""Convert MediaPipe Selfie Segmentation Landscape to an embedded fp16 ncnn model."""

from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import sys

MODEL_REPOSITORY = "onnx-community/mediapipe_selfie_segmentation_landscape"
MODEL_REVISION = "2497d5bec26c626c7b3c4edc6e1fefc21b64f6c3"
# pnnx leaves the FP16 ONNX convolutions as custom layers. Use the FP32 ONNX as
# a conversion intermediate so pnnx emits standard ncnn layers with FP16 weights.
MODEL_FILENAME = "onnx/model.onnx"

SCRIPT_DIR = Path(__file__).resolve().parent
REPOSITORY_ROOT = SCRIPT_DIR.parents[2]
WORK_DIR = SCRIPT_DIR / "build_work"
NCNN_BUILD_DIR = REPOSITORY_ROOT / "Models" / "ncnn" / "build_ncnn_tools"
MODEL_DIR = WORK_DIR / "mediapipe-selfie-segmentation-landscape"
MODEL_BASENAME = "mediapipe_selfie_segmentation_landscape_fp16_ncnn"


def run(*command: str | Path, cwd: Path | None = None, env: dict[str, str] | None = None) -> None:
    subprocess.run([str(argument) for argument in command], cwd=cwd, env=env, check=True)


def required_executable(name: str) -> Path:
    executable = shutil.which(name)
    if not executable:
        raise RuntimeError(
            f"Required executable '{name}' was not found. "
            f"Install {SCRIPT_DIR / 'requirements.txt'} in the current Python environment."
        )
    return Path(executable)


def download_model() -> None:
    environment = os.environ.copy()
    environment.pop("HF_TOKEN", None)
    environment["HF_HUB_DISABLE_IMPLICIT_TOKEN"] = "1"
    run(
        required_executable("hf"),
        "download",
        MODEL_REPOSITORY,
        MODEL_FILENAME,
        "--revision",
        MODEL_REVISION,
        "--local-dir",
        MODEL_DIR,
        env=environment,
    )


def convert_model() -> None:
    tools = NCNN_BUILD_DIR / "tools"
    environment = os.environ.copy()
    environment["PATH"] = os.pathsep.join((str(tools), environment["PATH"]))
    model_working_directory = MODEL_DIR / "onnx"
    generated_source_directory = REPOSITORY_ROOT / "src" / "EmbeddedModels"
    generated_source_directory.mkdir(parents=True, exist_ok=True)

    run(
        required_executable("pnnx"),
        "model.onnx",
        "inputshape=[1,3,144,256]",
        "fp16=1",
        "ncnnparam=model-fp16.ncnn.param",
        "ncnnbin=model-fp16.ncnn.bin",
        cwd=model_working_directory,
    )
    id_header = generated_source_directory / f"{MODEL_BASENAME}.id.h"
    memory_header = generated_source_directory / f"{MODEL_BASENAME}.mem.h"
    run(
        tools / "ncnn2mem",
        model_working_directory / "model-fp16.ncnn.param",
        model_working_directory / "model-fp16.ncnn.bin",
        id_header,
        memory_header,
        cwd=model_working_directory,
        env=environment,
    )


def main() -> None:
    WORK_DIR.mkdir(parents=True, exist_ok=True)
    download_model()
    convert_model()
    print(f"Generated Linux ncnn sources in {REPOSITORY_ROOT / 'src' / 'EmbeddedModels'}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"conversion failed: {error}", file=sys.stderr)
        raise SystemExit(1) from error
