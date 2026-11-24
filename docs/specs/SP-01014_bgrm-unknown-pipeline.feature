# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

@SP-01014
Feature: Unknown pipeline
  A pipeline selection that this build does not know never discloses the background.

  Background:
    Given libobs is started with the plugin and the OBS Studio filters loaded
    And a 1280 x 720 image source of the portrait test image
    And the source has the Background Removal filter only, with default settings
    And the source is shown in the program output, so it is active and showing
    And "the background is removed" means that more than 10% and less than 90% of the pixels have an alpha below 128

  @SP-01014.1
  Scenario: An unknown pipeline draws nothing
    When "v2_PipelineId" is set to 9999
    Then the source size is 0 x 0
    And nothing of the source is drawn
    And the plugin logs "[obs-backgroundremoval] Unknown rendering pipeline ID: 9999"

  @SP-01014.2
  Scenario: Selecting Auto recovers
    Given "v2_PipelineId" is 9999
    When "v2_PipelineId" is set to 0, Auto
    Then the source size is 1280 x 720
    And the background is removed
