# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

@SP-01009
Feature: Preceding crop filter
  A crop before Background Removal sets the size it processes.

  Background:
    Given libobs is started with the plugin and the OBS Studio filters loaded
    And a 1280 x 720 image source of the portrait test image
    And the source has a Crop/Pad filter cropping 140 pixels from the left and from the right, followed by the Background Removal filter
    And the source is shown in the program output, so it is active and showing

  @SP-01009.1
  Scenario Outline: The downscaled output is based on the cropped size
    When "v2_OutputDownscaleFactor" is set to <factor>
    Then the source size is <width> x <height>
    And the plugin logs "[obs-backgroundremoval] Creating the MediaPipe (Landscape) shader pipeline (1000x720, downscale factor <factor>)"
    And the background is removed

    Examples:
      | factor | width | height |
      | 1      | 1000  | 720    |
      | 2      | 500   | 360    |
