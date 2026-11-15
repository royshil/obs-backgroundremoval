# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

"""Compare the checked-in DirectML model with the pinned original on CPU."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess

import numpy as np
import onnx
import onnxruntime as ort
from huggingface_hub import hf_hub_download
from PIL import Image

import GenerateModel as generator


def coarse_mask(probabilities):
    """Strict background threshold, then a 5x5 box excluding padded pixels."""
    background = (probabilities.reshape(144, 256) < np.float32(0.42)).astype(np.float32)
    padded = np.pad(background, 2)
    valid = np.pad(np.ones_like(background), 2)
    total, count = np.zeros_like(background), np.zeros_like(background)
    for y in range(5):
        for x in range(5):
            total += padded[y:y + 144, x:x + 256]
            count += valid[y:y + 144, x:x + 256]
    return total / count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runner", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--offline", action="store_true")
    args = parser.parse_args()
    runner = args.runner.resolve(strict=True)
    args.output_dir.mkdir(parents=True, exist_ok=True)
    path = hf_hub_download(generator.MODEL_REPOSITORY, revision=generator.MODEL_REVISION,
                           filename=generator.MODEL_FILENAME, local_files_only=args.offline)
    source_bytes = Path(path).read_bytes()
    if hashlib.sha256(source_bytes).hexdigest() != generator.MODEL_SHA256:
        raise RuntimeError("Pinned ONNX SHA-256 mismatch")
    options = ort.SessionOptions()
    options.graph_optimization_level = ort.GraphOptimizationLevel.ORT_DISABLE_ALL
    options.intra_op_num_threads = 2
    options.log_severity_level = 3
    reference = ort.InferenceSession(source_bytes, options, providers=["CPUExecutionProvider"])
    lowered_model = generator.lower_model(onnx.load_from_string(source_bytes))
    lowered = ort.InferenceSession(lowered_model.SerializeToString(), options,
                                  providers=["CPUExecutionProvider"])
    shape = tuple(generator.INPUT_SHAPE)
    cases = {
        "zero": np.zeros(shape, dtype=np.float32),
        "half": np.full(shape, 0.5, dtype=np.float32),
        "ramp": np.linspace(0, 1, np.prod(shape), dtype=np.float32).reshape(shape),
        "random": np.random.default_rng(0).random(shape, dtype=np.float32),
    }
    portrait_path = Path(__file__).resolve().parents[3] / "tests/fixtures/0000-portrait-umireon.jpg"
    with Image.open(portrait_path) as image:
        rgb = np.asarray(image.convert("RGB").resize((256, 144), Image.Resampling.BILINEAR), dtype=np.float32)
        cases["portrait"] = (rgb.transpose(2, 0, 1)[None] / np.float32(255)).copy()
    report = []
    for name, values in cases.items():
        expected = reference.run(None, {reference.get_inputs()[0].name: values})[0]
        lowered_output = lowered.run(None, {lowered.get_inputs()[0].name: values})[0]
        np.testing.assert_allclose(lowered_output, expected, rtol=1e-4, atol=1e-4)
        input_path = args.output_dir / f"{name}.input.f32"
        raw_path = args.output_dir / f"{name}.raw.f32"
        mask_path = args.output_dir / f"{name}.mask.f32"
        values.astype("<f4").tofile(input_path)
        subprocess.run([str(runner), str(input_path), str(raw_path), str(mask_path)], check=True)
        raw = np.fromfile(raw_path, dtype="<f4").reshape(generator.OUTPUT_SHAPE)
        mask = np.fromfile(mask_path, dtype="<f4").reshape(144, 256)
        if not np.isfinite(raw).all() or not np.isfinite(mask).all():
            raise RuntimeError(f"{name}: non-finite model output")
        np.testing.assert_allclose(raw, expected, rtol=1e-3, atol=1e-3)
        expected_mask = coarse_mask(expected)
        actual_foreground, expected_foreground = mask <= 0.5, expected_mask <= 0.5
        union = np.count_nonzero(actual_foreground | expected_foreground)
        iou = np.count_nonzero(actual_foreground & expected_foreground) / union if union else 1.0
        mask_mae = float(np.mean(np.abs(mask - expected_mask)))
        row = {"input": name, "lowering_max_abs": float(np.max(np.abs(lowered_output - expected))),
               "raw_max_abs": float(np.max(np.abs(raw - expected))),
               "raw_rmse": float(np.sqrt(np.mean((raw - expected) ** 2))),
               "mask_mae": mask_mae, "mask_iou": iou,
               "foreground_pixels": int(np.count_nonzero(expected_foreground))}
        print(json.dumps(row), flush=True)
        if not ((mask >= 0).all() and (mask <= 1).all() and mask_mae <= 0.005 and iou >= 0.99):
            raise RuntimeError(f"{name}: mask postprocessing differs from the original model reference")
        report.append(row)
    (args.output_dir / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
