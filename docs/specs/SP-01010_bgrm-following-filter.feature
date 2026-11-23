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

  @SP-01010.1
  Scenario: The following filter receives the output
    Then the source size is 1280 x 720
    And the background is removed
