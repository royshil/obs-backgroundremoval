# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

@SP-01007
Feature: Odd source size
  An odd size is kept as it is.

  Background:
    Given libobs is started with the plugin and the OBS Studio filters loaded
    And a 1281 x 721 image source of the portrait test image
    And the source has the Background Removal filter only, with default settings
    And the source is shown in the program output, so it is active and showing

  @SP-01007.1
  Scenario: The output keeps the odd size
    Then the source size is 1281 x 721
    And the plugin logs "[obs-backgroundremoval] Creating the MediaPipe (Landscape) shader pipeline (1281x721)"
