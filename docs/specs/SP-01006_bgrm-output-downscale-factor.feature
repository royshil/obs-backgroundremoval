# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

@SP-01006
Feature: Output downscale factor
  The output downscale factor makes the filter output smaller than its input,
  while the frame is still processed at the input size.

  Background:
    Given libobs is started with the plugin and the OBS Studio filters loaded
    And a 1280 x 720 image source of the portrait test image
    And the source has the Background Removal filter only, with default settings
    And the source is shown in the program output, so it is active and showing

  @SP-01006.1
  Scenario Outline: The output size follows the output downscale factor
    When "v2_OutputDownscaleFactor" is set to <factor>
    Then the source size is <width> x <height>
    And the foreground mask of the output agrees with the mask at downscale factor 1, downscaled to <width> x <height>, on at least 99% of pixels
    And the plugin logs "[obs-backgroundremoval] Creating the MediaPipe (Landscape) shader pipeline (1280x720, downscale factor <factor>)"

    Examples:
      | factor | width | height |
      | 1      | 1280  | 720    |
      | 2      | 640   | 360    |
      | 4      | 320   | 180    |

  @SP-01006.2
  Scenario: Returning to downscale factor 1 recreates the pipeline
    Given "v2_OutputDownscaleFactor" is 4
    When "v2_OutputDownscaleFactor" is set to 1
    Then the source size is 1280 x 720
    And the plugin logs "[obs-backgroundremoval] Creating the MediaPipe (Landscape) shader pipeline (1280x720, downscale factor 1)"
