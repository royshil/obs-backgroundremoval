# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

"""Pack the MediaPipe Selfie Segmenter weights for the OBS shader pipeline.

Only the weights are generated. The network itself is written by hand in
src/BackgroundRemoval/MediaPipeLandscapeShader/MPLSPipeline.cpp and
src/BackgroundRemoval/MediaPipeLandscapeShader/MPLSEffect.cpp.

Every convolution layer gets its own RGBA16F weight texture and bias texture,
created once and set per pass. The model's weights are exact in IEEE half, so
nothing is lost; the generator fails if one is not. Tensors stay RGBA32F.
Texel (x, y) is listed row by row without padding. A slice is four consecutive
channels; channels beyond the channel count are zero.

Dense convolution (the 1x1 pointwise layers, and the 3x3 stem), kernel k:
    weights, (cin_slices * k * k * 4) x cout_slices texels:
        texel(((si * k + ky) * k + kx) * 4 + ci, so) = W[4 * so : 4 * so + 4, 4 * si + ci, ky, kx]
    so a texel holds the weights from one input channel to the four output channels of slice so.
Depthwise convolution, kernel k:
    weights, (k * k) x slices texels:
        texel(ky * k + kx, s) = W[4 * s : 4 * s + 4, 0, ky, kx]
Transposed convolution (16 -> 1, kernel 2, stride 2):
    weights, 4 x slices texels:
        texel(ky * 2 + kx, s) = W[4 * s : 4 * s + 4, 0, ky, kx]
Bias:
    bias, cout_slices x 1 texels:
        texel(s, 0) = B[4 * s : 4 * s + 4]
"""

from __future__ import annotations

import argparse
import hashlib
import pathlib
import subprocess
from dataclasses import dataclass

import clang_format
import numpy as np
import onnx
from huggingface_hub import hf_hub_download
from onnx import numpy_helper

MODEL_REPOSITORY = "onnx-community/mediapipe_selfie_segmentation_landscape"
MODEL_REVISION = "2497d5bec26c626c7b3c4edc6e1fefc21b64f6c3"
MODEL_FILENAME = "onnx/model.onnx"
MODEL_SHA256 = "7a0adcfdb1715d3b0ff0f61486d8a181c33f4a208343a5abfccbc6540e872d24"

OUTPUT_PATH = (
    pathlib.Path(__file__).resolve().parents[3] / "src/BackgroundRemoval/MediaPipeLandscapeShader/generated/MPLSPipeline.generated.cpp"
)


@dataclass(frozen=True)
class Layer:
    """One convolution layer; `weights` is [height, width, 4] and `bias` is [width, 4] in texels."""

    name: str
    weights: np.ndarray
    bias: np.ndarray


def slices(channels: int) -> int:
    return (channels + 3) // 4


def pad_channels(array: np.ndarray, axis: int) -> np.ndarray:
    padding = [(0, 0)] * array.ndim
    padding[axis] = (0, slices(array.shape[axis]) * 4 - array.shape[axis])
    return np.pad(array, padding)


def pack_dense(weight: np.ndarray) -> np.ndarray:
    cout, cin, k, _ = weight.shape
    w = pad_channels(pad_channels(weight, 0), 1)
    # [so, lane, si, ci, ky, kx] -> [so, si, ky, kx, ci, lane]
    w = w.reshape(slices(cout), 4, slices(cin), 4, k, k).transpose(0, 2, 4, 5, 3, 1)
    return w.reshape(slices(cout), -1, 4)


def pack_depthwise(weight: np.ndarray) -> np.ndarray:
    channels, _, k, _ = weight.shape
    w = pad_channels(weight[:, 0], 0)
    # [s, lane, ky, kx] -> [s, ky, kx, lane]
    return w.reshape(slices(channels), 4, k, k).transpose(0, 2, 3, 1).reshape(slices(channels), k * k, 4)


def pack_bias(bias: np.ndarray) -> np.ndarray:
    return pad_channels(bias, 0).reshape(-1, 4)


def layer_name(node: onnx.NodeProto) -> str:
    return node.input[1].removesuffix(".weight").replace(".", "_")


def pack_model(model: onnx.ModelProto) -> list[Layer]:
    initializers = {item.name: numpy_helper.to_array(item).astype(np.float32) for item in model.graph.initializer}
    layers: list[Layer] = []
    for node in model.graph.node:
        if node.op_type not in ("Conv", "ConvTranspose"):
            continue
        weight = initializers[node.input[1]]
        bias = initializers[node.input[2]]
        group = next((onnx.helper.get_attribute_value(a) for a in node.attribute if a.name == "group"), 1)
        if node.op_type == "ConvTranspose" or (group == weight.shape[0] and weight.shape[1] == 1):
            packed = pack_depthwise(weight)
        elif group == 1:
            packed = pack_dense(weight)
        else:
            raise RuntimeError(f"Unsupported convolution {node.name}")
        layers.append(Layer(layer_name(node), packed.astype(np.float32), pack_bias(bias).astype(np.float32)))
    return layers


def load_model(offline: bool = False) -> onnx.ModelProto:
    model_path = hf_hub_download(
        repo_id=MODEL_REPOSITORY, revision=MODEL_REVISION, filename=MODEL_FILENAME, local_files_only=offline
    )
    if hashlib.sha256(pathlib.Path(model_path).read_bytes()).hexdigest() != MODEL_SHA256:
        raise RuntimeError("The pinned MediaPipe Landscape model SHA-256 does not match")
    return onnx.load(model_path)


def half_bits(values: np.ndarray) -> np.ndarray:
    """Return the IEEE half bit patterns of float32 values that half represents exactly."""
    half = values.astype(np.float16)
    if not np.array_equal(half.astype(np.float32), values):
        raise RuntimeError("A weight or bias is not exact in IEEE half; RGBA16F would lose precision")
    return half.view(np.uint16)


def render_texels(values: np.ndarray) -> str:
    bits = half_bits(values.reshape(-1))
    return "\n".join(
        "\t" + ", ".join(f"0x{v:04x}" for v in bits[start : start + 4]) + ","
        for start in range(0, bits.size, 4)
    )


def render_arrays(layer: Layer) -> str:
    return f"""\
alignas(8) constexpr std::array<std::uint16_t, {layer.weights.size}> {layer.name}_weights{{{{
{render_texels(layer.weights)}
}}}};

alignas(8) constexpr std::array<std::uint16_t, {layer.bias.size}> {layer.name}_bias{{{{
{render_texels(layer.bias)}
}}}};
"""


def render_layer(layer: Layer) -> str:
    height, width, _ = layer.weights.shape
    return (f'extern constexpr Layer {layer.name}{{"{layer.name}", {layer.name}_weights.data(), {width}, {height}, '
            f'{layer.name}_bias.data(), {layer.bias.shape[0]}}};')


def render(layers: list[Layer]) -> str:
    arrays = "\n".join(render_arrays(layer) for layer in layers)
    definitions = "\n".join(render_layer(layer) for layer in layers)
    layer_list = ", ".join(f"&{layer.name}" for layer in layers)
    # REUSE-IgnoreStart
    return f"""\
// SPDX-FileCopyrightText: 2021 Google LLC
// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0
// Generated by Models/MediaPipeLandscape/Shader/GenerateWeights.py. Do not edit.
// Source: {MODEL_REPOSITORY}@{MODEL_REVISION}/{MODEL_FILENAME}

#include <array>
#include <cstdint>

#include <BackgroundRemoval/MediaPipeLandscapeShader/MPLSPipeline.hpp>

namespace BackgroundRemoval::MPLSDetail::MPLSWeights {{

namespace {{

{arrays}
}} // namespace

{definitions}

extern constexpr std::array<const Layer *, {len(layers)}> layers{{{{{layer_list}}}}};

}} // namespace BackgroundRemoval::MPLSDetail::MPLSWeights
"""
    # REUSE-IgnoreEnd


def format_source(source: str, path: pathlib.Path) -> str:
    """Format the source with the pinned clang-format 21 and the .clang-format files that apply to `path`."""
    result = subprocess.run(
        [clang_format.get_executable("clang-format"), f"--assume-filename={path.resolve()}"],
        input=source,
        capture_output=True,
        check=True,
        text=True,
    )
    return result.stdout


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--offline", action="store_true", help="Use the cached pinned model only")
    parser.add_argument("--output", type=pathlib.Path, default=OUTPUT_PATH)
    args = parser.parse_args()

    layers = pack_model(load_model(args.offline))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(format_source(render(layers), args.output), encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
