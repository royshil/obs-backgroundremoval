# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

@SP-01008
Feature: Preceding scale filter
  A size-changing filter before Background Removal sets the size it processes.

  Background:
    Given libobs is started with the plugin and the OBS Studio filters loaded
    And a 1280 x 720 image source of the portrait test image
    And the source has a Scaling/Aspect Ratio filter set to 640 x 360, followed by the Background Removal filter
    And the source is shown in the program output, so it is active and showing
    And "the background is removed" means that more than 10% and less than 90% of the pixels have an alpha below 128

  @SP-01008.1
  Scenario: The output has the preceding filter's size
    Then the source size is 640 x 360
    And the plugin logs "[obs-backgroundremoval] Creating the MediaPipe (Landscape) shader pipeline (640x360)"
    And the background is removed
