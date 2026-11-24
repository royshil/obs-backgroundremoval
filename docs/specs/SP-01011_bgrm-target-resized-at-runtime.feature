# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

@SP-01011
Feature: Target resized at runtime
  A change of the size Background Removal receives recreates its pipeline.

  Background:
    Given libobs is started with the plugin and the OBS Studio filters loaded
    And a 1280 x 720 image source of the portrait test image
    And the source has a Scaling/Aspect Ratio filter set to 640 x 360, followed by the Background Removal filter
    And the source is shown in the program output, so it is active and showing
    And "the background is removed" means that more than 10% and less than 90% of the pixels have an alpha below 128

  @SP-01011.1
  Scenario: Resizing the preceding filter recreates the pipeline
    Given the source size is 640 x 360
    When the Scaling/Aspect Ratio filter is set to 1280 x 720
    Then the source size is 1280 x 720
    And the plugin logs "[obs-backgroundremoval] Creating the MediaPipe (Landscape) shader pipeline (1280x720)"
    And the background is removed
