# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

"""Offline NumPy references; never run during CTest. Review regenerated fixtures."""

from pathlib import Path
import numpy as np


def input_values(scenario, shape):
    count = int(np.prod(shape))
    boundary = np.array([-100, -3.00001, -3, -2.99999, -1, 0, 1,
                         2.99999, 3, 3.00001, 10, 100], dtype=np.float32)
    if scenario == 0:
        return np.full(shape, .25, dtype=np.float32)
    if scenario == 1:
        return ((np.arange(count) % 24) - 12).astype(np.float32).reshape(shape)
    if scenario == 2:
        return np.resize(boundary, count).reshape(shape)
    x = np.zeros(count, dtype=np.float32)
    x[0 if scenario == 3 else -1] = 1
    return x.reshape(shape)


def reference(name, x):
    if name in ("HardSwish", "HardSwishFused"):
        return x * np.clip(x + 3, 0, 6) * np.float32(1 / 6)
    if name == "ReduceMean":
        return x.mean(axis=(2, 3), keepdims=True)
    if name in ("Add", "Mul"):
        bias = np.array([.5, -1.25], dtype=np.float32).reshape(1, 2, 1, 1)
        return x + bias if name == "Add" else x * bias
    if name == "Relu":
        return np.maximum(x, 0)
    if name == "Sigmoid":
        return (1 / (1 + np.exp(-x.astype(np.float64)))).astype(np.float32)
    if name == "Clip":
        return np.clip(x, 0, 6)
    if name == "Resize":
        h, w = x.shape[2:]
        yy = (np.arange(h * 2) + .5) / 2 - .5
        xx = (np.arange(w * 3) + .5) / 3 - .5
        iy, ix = np.floor(yy).astype(int), np.floor(xx).astype(int)
        fy, fx = yy - iy, xx - ix
        def at(dy, dx):
            return x[:, :, np.clip(iy + dy, 0, h - 1)[:, None],
                     np.clip(ix + dx, 0, w - 1)[None, :]]
        top = at(0, 0) * (1 - fx) + at(0, 1) * fx
        bottom = at(1, 0) * (1 - fx) + at(1, 1) * fx
        return top * (1 - fy[:, None]) + bottom * fy[:, None]
    if name == "PointwiseConvTranspose":
        weights = ((np.arange(16)[:, None] * 4 + np.arange(4)) % 7 - 3) / 16
        projected = np.einsum("nchw,cp->nphw", x, weights) + .25
        return projected.reshape(1, 1, 2, 2, 72, 128).transpose(0, 1, 4, 2, 5, 3).reshape(1, 1, 144, 256)
    depthwise = name in ("DepthwiseConv", "DepthwiseConvRelu", "MPLD10DepthwiseConv2dRelu")
    transpose = name == "ConvTranspose"
    channels = x.shape[1]
    kernel = 2 if transpose else 3
    weights = ((np.arange(channels * (1 if depthwise else channels) * kernel * kernel) % 7) - 3) / 16
    weights = weights.reshape(channels, 1 if depthwise else channels, kernel, kernel)
    bias = np.where(np.arange(channels) % 2, -.5, .25)
    h, w = x.shape[2:]
    out = np.zeros((1, channels, h * 2 if transpose else h, w * 2 if transpose else w), dtype=np.float64)
    if transpose:
        for c in range(channels):
            for o in range(channels):
                for ky in range(kernel):
                    for kx in range(kernel):
                        out[:, o, ky::2, kx::2] += x[:, c] * weights[c, o, ky, kx]
    else:
        padded = np.pad(x, ((0, 0), (0, 0), (1, 1), (1, 1)))
        for o in range(channels):
            for c in ([o] if depthwise else range(channels)):
                for ky in range(kernel):
                    for kx in range(kernel):
                        out[:, o] += padded[:, c, ky:ky+h, kx:kx+w] * weights[o, 0 if depthwise else c, ky, kx]
    out += bias.reshape(1, channels, 1, 1)
    return np.maximum(out, 0) if "Relu" in name else out


if __name__ == "__main__":
    destination = Path(__file__).with_name("operators")
    destination.mkdir(exist_ok=True)
    names = ["HardSwish", "HardSwishFused", "ReduceMean", "Resize", "Add", "Mul",
             "Relu", "Sigmoid", "Clip", "Conv", "DepthwiseConv", "ConvTranspose",
             "ConvRelu", "DepthwiseConvRelu", "PointwiseConvTranspose", "MPLD10DepthwiseConv2dRelu"]
    for name in names:
        shape = (1, 16, 72, 128) if name in ("PointwiseConvTranspose", "MPLD10DepthwiseConv2dRelu") else (1, 2, 3, 4)
        with (destination / f"{name}.f32").open("wb") as fixture:
            for scenario in range(5):
                x = input_values(scenario, shape)
                reference(name, x).astype("<f4").tofile(fixture)
