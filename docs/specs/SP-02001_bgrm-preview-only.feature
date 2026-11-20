# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

@SP-02001
Feature: Preview only
  Background Removal works in the previews of OBS Studio, not only in the program output.

  Background:
    Given a scene contains a 1280 x 720 image source showing a person in front of a background
    And the source has the Background Removal filter only, with default settings

  @SP-02001.1
  Scenario: Studio Mode preview
    Given Studio Mode is enabled
    And the scene is shown in the preview but not in the program output
    Then the preview shows the source with the background removed

  @SP-02001.2
  Scenario: Filter properties preview
    Given the scene is not shown in the program output
    When the user opens the filters of the source
    Then the filter preview shows the source with the background removed
