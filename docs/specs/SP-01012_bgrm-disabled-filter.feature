# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

@SP-01012
Feature: Disabled filter
  A disabled Background Removal filter is bypassed and keeps its pipeline as a hot standby.

  Background:
    Given libobs is started with the plugin and the OBS Studio filters loaded
    And a 1280 x 720 image source of the portrait test image
    And the source has the Background Removal filter only, with default settings
    And the source is shown in the program output, so it is active and showing
    And "the background is removed" means that more than 10% and less than 90% of the pixels have an alpha below 128

  @SP-01012.1
  Scenario: Disabling the filter restores the source
    When the Background Removal filter is disabled
    Then the source size is 1280 x 720
    And the source shows the unfiltered image, within 1

  @SP-01012.2
  Scenario: Enabling the filter again draws at once
    Given the Background Removal filter is disabled
    When the Background Removal filter is enabled
    Then the plugin logs no pipeline creation
    And the source size is 1280 x 720
    And the first frame rendered after enabling has the background removed

  @SP-01012.3
  Scenario: A disabled filter follows changes
    Given the Background Removal filter is disabled
    When "v2_PipelineId" is set to 17
    Then the plugin logs "[obs-backgroundremoval] Creating the MediaPipe (Landscape) shader pipeline (1280x720)"
    And the source size is 1280 x 720
    When the Background Removal filter is enabled
    Then the plugin logs no further pipeline creation
    And the source size is 1280 x 720
