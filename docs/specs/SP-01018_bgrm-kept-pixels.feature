# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

@SP-01018
Feature: Kept pixels
  The filter keeps the foreground as it is and only cuts the background away.

  Background:
    Given libobs is started with the plugin and the OBS Studio filters loaded
    And a 1280 x 720 image source of the portrait test image
    And the source has the Background Removal filter only, with default settings
    And the source is shown in the program output, so it is active and showing

  @SP-01018.1
  Scenario: Kept pixels are unchanged
    Then every opaque pixel of the source has the color of the unfiltered image, within 1
