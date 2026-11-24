# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

@SP-01010
Feature: Following filter
  A filter after Background Removal receives its output.

  Background:
    Given libobs is started with the plugin and the OBS Studio filters loaded
    And a 1280 x 720 image source of the portrait test image
    And the source has the Background Removal filter, followed by a Color Correction filter with default settings
    And the source is shown in the program output, so it is active and showing
    And "the background is removed" means that more than 10% and less than 90% of the pixels have an alpha below 128

  @SP-01010.1
  Scenario: The following filter receives the output
    Then the source size is 1280 x 720
    And the background is removed
