# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

@SP-01016
Feature: Blur strength
  Strength sets how much the background is blurred.

  Background:
    Given libobs is started with the plugin and the OBS Studio filters loaded
    And a 1280 x 720 image source of the portrait test image
    And the source has the Background Removal filter only, with "v2_BlurBg" true and "v2_BlurBg_FocusRatio" -100
    And the source is shown in the program output, so it is active and showing

  @SP-01016.1
  Scenario: Strength 0 leaves the frame sharp
    When "v2_BlurBg_Factor" is set to 0
    Then the source shows the unfiltered image, within 1
    And every pixel of the source is opaque

  @SP-01016.2
  Scenario: The output is opaque
    When "v2_BlurBg_Factor" is set to 8
    Then every pixel of the source is opaque

  @SP-01016.3
  Scenario: More strength blurs the background more
    When "v2_BlurBg_Factor" is set to 2, 4, 8 and 20 in turn
    Then the mean change of the background from the unfiltered image increases at every step
    And the mean change of the person stays below the mean change of the background

  @SP-01016.4
  Scenario: Strength is continuous
    When "v2_BlurBg_Factor" is set to 4.0 and then to 4.1
    Then the two outputs differ by less than they differ from the output at 5
