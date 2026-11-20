# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

@SP-01007
Feature: Odd source size
  Scaling rounds odd sizes up.

  Background:
    Given libobs is started with the plugin and the OBS Studio filters loaded
    And a 1281 x 721 image source of the portrait test image
    And the source has the Background Removal filter only, with default settings
    And the source is shown in the program output, so it is active and showing

  @SP-01007.1
  Scenario Outline: The output size rounds up
    When "v2_OutputDownscaleFactor" is set to <factor>
    Then the source size is <width> x <height>
    And the plugin logs "[obs-backgroundremoval] Creating the MediaPipe (Landscape) shader pipeline (1281x721, downscale factor <factor>)"

    Examples:
      | factor | width | height |
      | 1      | 1281  | 721    |
      | 2      | 641   | 361    |
      | 4      | 321   | 181    |
