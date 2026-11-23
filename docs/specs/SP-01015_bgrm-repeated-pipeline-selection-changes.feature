# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

@SP-01015
Feature: Repeated pipeline selection changes
  Each change of the pipeline selection recreates the pipeline, also between
  Auto and the pipeline it selects. That this does not stall rendering is
  SP-03001.

  Background:
    Given libobs is started with the plugin and the OBS Studio filters loaded
    And a 1280 x 720 image source of the portrait test image
    And the source has the Background Removal filter only, with default settings
    And the source is shown in the program output, so it is active and showing

  @SP-01015.1
  Scenario: Changing the pipeline selection recreates the pipeline
    When "v2_PipelineId" is changed 10 times, alternating between 17 and 0
    Then the plugin logs a pipeline creation 10 times
