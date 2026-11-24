# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

@SP-01020
Feature: Translucent source
  The filter keeps the transparency of its target. The scene is compared as OBS
  composites it into the program output, with the source without Background
  Removal as the reference.

  Background:
    Given libobs is started with the plugin and the OBS Studio filters loaded
    And a 1280 x 720 image source of the portrait test image
    And the source has a Color Correction filter with "opacity" 0.5, followed by the Background Removal filter
    And the source is shown in the program output, so it is active and showing

  @SP-01020.1
  Scenario: Kept pixels keep their color and transparency
    When the Background Removal filter has default settings
    Then every pixel of the scene that is not fully transparent has the color and the alpha of the scene without Background Removal, within 2

  @SP-01020.2
  Scenario: Blur strength 0 leaves the source as it is
    When "v2_BlurBg" is true and "v2_BlurBg_Factor" is 0
    Then the scene shows the scene without Background Removal, within 2

  @SP-01020.3
  Scenario: The blur keeps the transparency
    When "v2_BlurBg" is true and "v2_BlurBg_Factor" is 8
    Then the alpha of every pixel of the scene is that of the scene without Background Removal, within 2
