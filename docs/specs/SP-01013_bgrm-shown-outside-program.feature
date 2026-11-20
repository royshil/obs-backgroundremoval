# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

@SP-01013
Feature: Shown outside the program output
  Background Removal works wherever its source is shown, not only in the program output.

  Background:
    Given libobs is started with the plugin and the OBS Studio filters loaded
    And a 1280 x 720 image source of the portrait test image
    And the source has the Background Removal filter only, with default settings

  @SP-01013.1
  Scenario: Showing but not active
    Given the source is showing but not active, as in a preview
    When the source is rendered
    Then the background is removed
