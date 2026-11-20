# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

@SP-01018
Feature: Mask adjustment
  Mask adjustment grows or shrinks the kept foreground.

  Background:
    Given libobs is started with the plugin and the OBS Studio filters loaded
    And a 1280 x 720 image source of the portrait test image
    And the source has the Background Removal filter only, with default settings
    And the source is shown in the program output, so it is active and showing

  @SP-01018.1
  Scenario: Positive adjustment keeps more, negative keeps less
    When "v2_MaskAdjustment" is set to -30, 0 and 30 in turn
    Then the share of opaque pixels increases at every step

  @SP-01018.2
  Scenario: Kept pixels are unchanged
    When "v2_MaskAdjustment" is set to 0
    Then every opaque pixel of the source has the color of the unfiltered image
