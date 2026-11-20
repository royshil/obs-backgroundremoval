# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

@SP-01002
Feature: Dual filter kernel
  Dual filtering down to L levels and back is a linear filter. Its impulse response
  is a well-behaved blur: normalized, non-negative, centered on the impulse,
  nearly isotropic, and the same wherever the impulse is.

  Background:
    Given texel i of an axis of N texels has its center at uv (i + 0.5) / N
    And textures are sampled bilinearly with clamp addressing
    And halfpixel is half a texel of the lower resolution of each pass

  @SP-01002.1
  Scenario Outline: Impulse response on a power-of-two frame
    Given a 256 x 256 frame with a single texel set to 1
    When it is blurred by Dual filtering down to <levels> levels and back
    Then the response sums to 1 within 1e-9
    And no texel of the response is negative
    And the centroid of the response is the impulse's position within 1e-9 texels
    And the standard deviation per axis is <sigma> texels within 0.01
    And the standard deviations along the two diagonals differ from the one along the axes by at most <anisotropy>
    And moving the impulse within one texel of level <levels> changes the response by at most <shape> in L1

    Examples:
      | levels | sigma | anisotropy | shape |
      | 1      | 1.68  | 5%         | 0.18  |
      | 2      | 3.76  | 3%         | 0.09  |
      | 3      | 7.71  | 2%         | 0.07  |
      | 4      | 15.52 | 1%         | 0.07  |

  @SP-01002.2
  Scenario Outline: A uniform frame stays uniform
    Given a <width> x <height> frame where every texel is 1
    When it is blurred by Dual filtering down to <levels> levels and back, halving sizes rounded up
    Then every texel of the result is 1 within 1e-9, at the edges too

    Examples:
      | width | height | levels |
      | 1280  | 720    | 5      |
      | 1920  | 1080   | 5      |
      | 1281  | 721    | 4      |

  @SP-01002.3
  Scenario: The other reading of halfpixel is rejected
    Given halfpixel is half a texel of the higher resolution of each pass instead
    When an impulse on a 256 x 256 frame is blurred down to 4 levels and back
    Then the centroid moves by more than 1 texel for some positions of the impulse
