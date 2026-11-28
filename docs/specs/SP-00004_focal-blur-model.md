---
# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0
kind: Design principle
platforms: All
---

# SP-00004: Focal blur model

## Principle

The background blur behaves like a camera focusing somewhere in the scene, through two settings
that are quantities of this filter, not physical ones: Strength and Focus ratio.

## Rationale

Mapping the settings to physical quantities such as aperture or distance would promise behavior
that a segmentation mask cannot deliver. Two filter-specific settings that behave like a camera are
honest and simple to use.

## Requirements

1. Strength s, in [0, 20] and continuous, blurs as much, by variance, as s iterations of the Kawase
   blur this filter used before: s (2 s^2 + 1) / 6 texels squared per axis. Strength 0 leaves the
   frame sharp.
2. Focus ratio, a percentage in [-100%, 100%], is where the frame stays sharp in mask space:
   -100% on the person, 100% on the background, and 0% at the point where the two balance, linear
   on each side. The default is -100%. The blur at a pixel grows with its distance from there.
   The person lies at 0 and the background at 1, and the distance of a pixel is the blend of the
   distances of the two by its foreground probability. The outline between them is not a depth of
   its own, so it is never in focus by itself.
   Each pipeline places the ratio on its own mask, since where the person, the background and the
   balance between them lie there depends on the distribution of the model's output.
3. The distance from the focus point is computed from the foreground probability alone, which is
   the refined mask transformed. The transform is currently the identity.
4. The output is the blurred frame alone, with the transparency of the frame blurred along with
   it: an opaque source stays opaque and a translucent one stays translucent. The person is not
   composited back sharp, so that the focus point keeps its meaning.
5. The halo around the person is kept. It shows the user that the focus ratio is off.
6. There is no focus depth setting.
7. The upsampling stops at pyramid level 2. The final draw blends the frame with level 2, magnified
   bilinearly, by the weight of levels 1 and 2 taken as one, in linear space; the pyramid itself
   blurs the stored values. A pixel in focus keeps the frame's resolution. The blur is approximate
   below the resolution of level 2: in particular, a light blur comes out lighter than requirement 1
   gives. This is the variation of SP-00003 requirement 6, and there is no setting for it.

## Verified by

- SP-01003: Blur variance
- SP-01016: Blur strength
- SP-01017: Blur focus ratio
- SP-01020: Translucent source
