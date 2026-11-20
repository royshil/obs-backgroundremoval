---
# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0
---

# SP-00003: Published techniques, as published

- Kind: Design principle
- Platforms: All

## Principle

Image processing such as blurring and mask refinement uses published, well-known techniques in the
form they were published, without novel variations.

## Rationale

A published technique comes with prior art and known properties, and its behavior can be checked
against its description. A variation would need its own verification and has neither.

## Requirements

1. The blur is Dual filtering as presented by Marius Bjørge, "Bandwidth-Efficient Rendering",
   SIGGRAPH 2015. It uses the 5-tap downsample and 8-tap upsample with the weights and sample
   positions of the slides. halfpixel is half a texel of the lower resolution of each pass, as the
   slide figure shows.
2. Pyramid levels halve the size of the level above, rounded up, and are at least 1 x 1.
3. Varying the blur per pixel blends adjacent pyramid levels, as mipmap-based depth-of-field
   rendering does.
4. Mask refinement thresholds the probability and averages a 5 x 5 window without counting padding,
   as ONNX AveragePool with `count_include_pad = 0`.
5. Where a published description is ambiguous, the reading is settled by checking the result's
   properties (SP-01002) and recorded in the code.

## Verified by

- SP-01002: Dual filter kernel
- SP-01004: Mask refinement
