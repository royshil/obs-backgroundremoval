---
# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0
---

# SP-00004: Focal blur model

- Kind: Design principle
- Platforms: All

## Principle

The background blur behaves like a camera focusing somewhere in the scene, through two settings
that are quantities of this filter, not physical ones: Strength and Focus point.

## Rationale

Mapping the settings to physical quantities such as aperture or distance would promise behavior
that a segmentation mask cannot deliver. Two filter-specific settings that behave like a camera are
honest and simple to use.

## Requirements

1. Strength s, in [0, 20] and continuous, blurs as much, by variance, as s iterations of the Kawase
   blur this filter used before: s (2 s^2 + 1) / 6 texels squared per axis. Strength 0 leaves the
   frame sharp.
2. Focus point, in [0, 1], is where the frame stays sharp in mask space: 0 on the person and 1 on
   the background. The blur at a pixel grows with its distance from the focus point.
3. The distance from the focus point is computed from the refined mask alone, once per frame at
   the size of the mask.
4. The output is the blurred frame alone, opaque. The person is not composited back sharp, so that
   the focus point keeps its meaning.
5. The halo around the person is kept. It shows the user that the threshold or focus point is off.
6. There is no focus depth setting.

## Verified by

- SP-01003: Blur variance
- SP-01016: Blur strength
- SP-01017: Blur focus point
