# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

@SP-01001
Feature: Output size
  The output size for a downscale factor halves each side once per factor of two,
  rounding up, and is at least 1; the same rule halves the blur pyramid.

  @SP-01001.1
  Scenario Outline: Downscaled size of one side
    Given a side of <size> pixels
    When it is downscaled by the factor <factor>
    Then it is <downscaled> pixels

    Examples:
      | size | factor | downscaled |
      | 1280 | 1      | 1280       |
      | 1280 | 2      | 640        |
      | 1280 | 4      | 320        |
      | 1281 | 2      | 641        |
      | 1281 | 4      | 321        |
      | 721  | 2      | 361        |
      | 721  | 4      | 181        |
      | 1    | 4      | 1          |
      | 2    | 4      | 1          |
      | 3    | 4      | 1          |
      | 0    | 4      | 0          |

  @SP-01001.2
  Scenario Outline: Only 1, 2 and 4 are valid downscale factors
    Given the Downscale factor setting is stored as <stored>
    When the filter reads its settings
    Then the downscale factor is <factor>

    Examples:
      | stored | factor |
      | 1      | 1      |
      | 2      | 2      |
      | 4      | 4      |
      | 0      | 1      |
      | 3      | 1      |
      | 8      | 1      |
      | -2     | 1      |
