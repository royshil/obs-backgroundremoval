# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

@SP-03001
Feature: Rendering does not stall
  Recreating the pipeline is cheap enough to change the pipeline selection while streaming.

  Background:
    Given libobs is started with the plugin and the OBS Studio filters loaded
    And a 1280 x 720 image source of the portrait test image
    And the source has the Background Removal filter only, with default settings
    And the source is shown in the program output, so it is active and showing
    And the video thread of libobs has rendered for at least 10 seconds

  @SP-03001.1
  Scenario: Changing the pipeline selection does not stall rendering
    When "v2_PipelineId" is changed 10 times, alternating between 17 and 0
    Then libobs skips no more than 1 rendered frame per change

  @SP-03001.2
  Scenario: A second filter does not stall rendering
    When the Background Removal filter is added to a second 1280 x 720 image source shown in the program output
    Then libobs skips no more than 1 rendered frame
