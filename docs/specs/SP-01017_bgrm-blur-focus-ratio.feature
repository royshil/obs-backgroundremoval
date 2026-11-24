# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

@SP-01017
Feature: Blur focus ratio
  Focus ratio sets where the frame stays sharp as a percentage, from the person
  at -100% through the point where the two balance at 0% to the background at
  100%.

  Background:
    Given libobs is started with the plugin and the OBS Studio filters loaded
    And a 1280 x 720 image source of the portrait test image
    And the source has the Background Removal filter only, with "v2_BlurBg" true and "v2_BlurBg_Factor" 8
    And the source is shown in the program output, so it is active and showing

  @SP-01017.1
  Scenario: Focus on the person
    When "v2_BlurBg_FocusRatio" is set to -100
    Then the mean change of the person from the unfiltered image is below 1
    And the mean change of the background is above 3

  @SP-01017.2
  Scenario: Focus on the background
    When "v2_BlurBg_FocusRatio" is set to 100
    Then the mean change of the background from the unfiltered image is below 1
    And the mean change of the person is above 3

  @SP-01017.3
  Scenario: Moving the focus moves the sharpness
    When "v2_BlurBg_FocusRatio" is set to -100, 0 and 100 in turn
    Then the mean change of the person increases at every step
    And the mean change of the background decreases at every step

  @SP-01017.4
  Scenario: At the balance point the outline is blurred like the rest
    When "v2_BlurBg_FocusRatio" is set to 0
    Then the mean change of the pixels within 4 pixels of the outline of the person is above both the mean change of the person and that of the background away from the outline
