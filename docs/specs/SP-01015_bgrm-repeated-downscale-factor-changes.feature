# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

@SP-01015
Feature: Repeated downscale factor changes
  Each change of the output downscale factor recreates the pipeline. That this does not stall
  rendering is SP-03001.

  Background:
    Given libobs is started with the plugin and the OBS Studio filters loaded
    And a 1280 x 720 image source of the portrait test image
    And the source has the Background Removal filter only, with default settings
    And the source is shown in the program output, so it is active and showing
    And the video thread of libobs has rendered for at least 10 seconds

  @SP-01015.1
  Scenario: Changing the output downscale factor recreates the pipeline
    When "v2_OutputDownscaleFactor" is changed 10 times, cycling through 2, 4 and 1
    Then the plugin logs a pipeline creation 10 times
