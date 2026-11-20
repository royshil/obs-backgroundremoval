# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

"""Write the references of SP-01004.4 and SP-01005.2 next to this script.

- portrait-umireon-256x144.png: the portrait test image at the model's input size, the input that
  both the shader pipeline and ONNX Runtime evaluate, so that resizing never differs between them.
- portrait-umireon-256x144.probability.pgm: the foreground probability ONNX Runtime infers from it
  with the pinned model, in 8 bits as the shader pipeline stores it.
- portrait-umireon-256x144.refined.pgm: that probability refined by a NumPy reference of the
  RefineMask pass: the share of the texels at least 0.5 in the 5 x 5 window, counting only the
  texels inside the mask, in 8 bits.

Usage: python -m pip install -r tests/data/requirements.txt
       python tests/data/GenerateMPLSReference.py
"""

import pathlib

import numpy as np
import onnxruntime
from huggingface_hub import hf_hub_download
from PIL import Image

MODEL_REPOSITORY = "onnx-community/mediapipe_selfie_segmentation_landscape"
MODEL_REVISION = "2497d5bec26c626c7b3c4edc6e1fefc21b64f6c3"
MODEL_FILENAME = "onnx/model.onnx"

WIDTH = 256
HEIGHT = 144
THRESHOLD = 0.5

DATA = pathlib.Path(__file__).resolve().parent


def write_pgm(path: pathlib.Path, values: np.ndarray) -> None:
    height, width = values.shape
    path.write_bytes(f"P5\n{width} {height}\n255\n".encode() + values.astype(np.uint8).tobytes())


def refine(probability: np.ndarray) -> np.ndarray:
    foreground = (probability >= THRESHOLD).astype(np.float64)
    inside = np.ones_like(foreground)
    padded_foreground = np.pad(foreground, 2)
    padded_inside = np.pad(inside, 2)
    total = np.zeros_like(foreground)
    count = np.zeros_like(foreground)
    for dy in range(5):
        for dx in range(5):
            total += padded_foreground[dy : dy + HEIGHT, dx : dx + WIDTH]
            count += padded_inside[dy : dy + HEIGHT, dx : dx + WIDTH]
    return total / count


def main() -> None:
    image = Image.open(DATA / "portrait-umireon.jpg").convert("RGB").resize((WIDTH, HEIGHT), Image.LANCZOS)
    image.save(DATA / "portrait-umireon-256x144.png")

    model = hf_hub_download(repo_id=MODEL_REPOSITORY, revision=MODEL_REVISION, filename=MODEL_FILENAME)
    session = onnxruntime.InferenceSession(model, providers=["CPUExecutionProvider"])
    pixels = (np.asarray(image, dtype=np.float32) / 255.0).transpose(2, 0, 1)[np.newaxis]
    alphas = session.run(None, {session.get_inputs()[0].name: pixels})[0][0, 0]

    probability = np.clip(np.rint(alphas * 255.0), 0, 255).astype(np.uint8)
    write_pgm(DATA / "portrait-umireon-256x144.probability.pgm", probability)

    refined = refine(probability.astype(np.float64) / 255.0)
    write_pgm(DATA / "portrait-umireon-256x144.refined.pgm", np.rint(refined * 255.0))


if __name__ == "__main__":
    main()
