# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

@SP-01019
Feature: Settings compatibility
  Settings saved by earlier versions keep working.

  Background:
    Given libobs is started with the plugin and the OBS Studio filters loaded
    And a 1280 x 720 image source of the portrait test image
    And the source is shown in the program output, so it is active and showing

  @SP-01019.1
  Scenario: An integer blur strength is read as the same strength
    Given the Background Removal filter is created from saved settings with "v2_BlurBg" true and "v2_BlurBg_Factor" stored as the integer 8
    Then the output equals the output with "v2_BlurBg_Factor" 8.0

  @SP-01019.2
  Scenario: The removed focus depth is ignored
    Given the Background Removal filter is created from saved settings with "v2_BlurBg_FocusDepth" stored as 0.2
    Then the output equals the output of the same settings without "v2_BlurBg_FocusDepth"

  @SP-01019.3
  Scenario: The removed output downscale factor is ignored
    Given the Background Removal filter is created from saved settings with "v2_OutputDownscaleFactor" stored as 4
    Then the source size is 1280 x 720

  @SP-01019.4
  Scenario: The removed mask adjustment is ignored
    Given the Background Removal filter is created from saved settings with "v2_MaskAdjustment" stored as 30
    Then the output equals the output of the same settings without "v2_MaskAdjustment"

  @SP-01019.5
  Scenario: A saved focus point is read as the same focus ratio
    Given the Background Removal filter is created from saved settings with "v2_BlurBg" true and "v2_BlurBg_FocusPoint" stored as 0.75
    Then the output equals the output with "v2_BlurBg_FocusRatio" 50
