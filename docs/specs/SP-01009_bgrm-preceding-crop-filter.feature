# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

@SP-01009
Feature: Preceding crop filter
  A crop before Background Removal sets the size it processes.

  Background:
    Given libobs is started with the plugin and the OBS Studio filters loaded
    And a 1280 x 720 image source of the portrait test image
    And the source has a Crop/Pad filter cropping 140 pixels from the left and from the right, followed by the Background Removal filter
    And the source is shown in the program output, so it is active and showing
    And "the background is removed" means that more than 10% and less than 90% of the pixels have an alpha below 128

  @SP-01009.1
  Scenario: The output has the cropped size
    Then the source size is 1000 x 720
    And the plugin logs "[obs-backgroundremoval] Creating the MediaPipe (Landscape) shader pipeline (1000x720)"
    And the background is removed
