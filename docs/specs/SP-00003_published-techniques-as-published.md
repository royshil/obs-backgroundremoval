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
form they were published, without novel variations, except for the one variation that requirement 6
records with its grounds.

## Rationale

A published technique comes with prior art and known properties, and its behavior can be checked
against its description. A variation would need its own verification and has neither.

## Requirements

1. The blur is Dual filtering as presented by Marius Bjørge, "Bandwidth-Efficient Rendering",
    SIGGRAPH 2015. It uses the 5-tap downsample and 8-tap upsample with the weights and sample
    positions of the slides. halfpixel is half a texel of the lower resolution of each pass, as the
    slide figure shows. The two finest upsample passes are replaced as requirement 6 states.
2. Pyramid levels halve the size of the level above, rounded up, and are at least 1 x 1.
3. Varying the blur per pixel blends adjacent pyramid levels, as mipmap-based depth-of-field
    rendering does, except between the frame and level 2 as requirement 6 states.
4. Mask refinement thresholds the probability and averages a 5 x 5 window without counting padding,
    as ONNX AveragePool with `count_include_pad = 0`.
5. Where a published description is ambiguous, the reading is settled by checking the result's
    properties (SP-01002) and recorded in the code.
6. The one variation: the upsampling stops at pyramid level 2, and the final draw blends the frame
    with level 2, magnified bilinearly, by the weight of levels 1 and 2 taken as one (SP-00004
    requirement 7). Each upsample pass writes a quarter of the pixels of the one above it, so the
    two finest are most of the upsampling. Measured on Apple silicon, skipping them saved about 38%
    of the compositor's GPU time at 3840 x 2160, and stopping deeper saved under 5% more. On the
    portrait test image the result differs from the full upsampling by at most 3 in the 99th
    percentile at strength 8 and above, and a light blur comes out lighter. The variation is
    checked through the cases of the blur, SP-01016 and SP-01017.

## Verified by

- SP-01002: Dual filter kernel
- SP-01004: Mask refinement
- SP-01016: Blur strength
- SP-01017: Blur focus ratio
