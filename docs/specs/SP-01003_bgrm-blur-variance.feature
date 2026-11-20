# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

@SP-01003
Feature: Blur variance
  Strength s blurs as much, by variance, as s iterations of the Kawase blur
  this filter used before. The weight of each pyramid level is the share of
  its variance that the target still needs.

  Background:
    Given the sampling conventions of SP-01002
    And variances are per axis, in texels squared, of the impulse response at the center of a 512 x 512 frame, large enough that the widest blur stays clear of the edges

  @SP-01003.1
  Scenario Outline: Kawase variance
    Given the Kawase blur whose iteration i averages four diagonal taps at (i + 0.5) texels
    When an impulse is blurred by <iterations> iterations
    Then the variance is <iterations> (2 <iterations>^2 + 1) / 6 within a relative 1e-9, that is <variance>

    Examples:
      | iterations | variance |
      | 1          | 0.5      |
      | 4          | 22       |
      | 8          | 172      |
      | 20         | 2670     |

  @SP-01003.2
  Scenario Outline: Dual filter variance
    When an impulse is blurred by Dual filtering down to <levels> levels and back
    Then the variance is (17 / 18) (4^<levels> - 1) within a relative 1e-9

    Examples:
      | levels |
      | 1      |
      | 2      |
      | 3      |
      | 4      |
      | 5      |
      | 6      |

  @SP-01003.3
  Scenario Outline: The level weights reproduce the Kawase variance
    Given strength <strength>
    When the pyramid goes down to the fewest levels whose variance reaches the strength's
    And upsampling level k blends with level k - 1 by clamp((s (2 s^2 + 1) - (17 / 3) (4^(k - 1) - 1)) / (17 4^(k - 1)), 0, 1)
    Then the pyramid has <levels> levels
    And the variance of the impulse response equals the Kawase variance of the strength within a relative 1e-9

    Examples:
      | strength | levels |
      | 0.25     | 1      |
      | 1        | 1      |
      | 2.5      | 2      |
      | 4        | 3      |
      | 8        | 4      |
      | 20       | 6      |

  @SP-01003.4
  Scenario: Strength 0 does not blur
    Given strength 0
    When the frame is blurred
    Then the pyramid has 0 levels
    And the result is the frame
