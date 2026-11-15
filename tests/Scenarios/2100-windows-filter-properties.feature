# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

Feature: Windows background-removal filter properties
  Background:
    Given the Windows Background Removal filter uses the en-US locale
    And its properties are displayed

  Scenario: ST-2100.1 Show current controls and defaults
    Then the General group contains Pipeline, Mask adjustment, and Motion suppression
    And Pipeline offers Auto (MediaPipe) and MediaPipe (Landscape) - DirectML (Basic)
    And Auto is selected by default
    And Mask adjustment and Motion suppression range from -50 to 50 with default 0
    And Blur Background is an unchecked optional group
    And the version notice, auto-configuration wizard, and About action are available

  Scenario: ST-2100.2 Measure and fine-tune motion suppression
    Given an active video source provides motion samples
    When the user opens the auto-configuration wizard and starts measurement
    Then Apply, OK, and the adjustment controls are disabled during measurement
    When the three-second measurement finishes
    Then the recommendation is editable from -50 to 50
    When the user changes the value and selects Apply
    Then the setting and filter properties are updated and the wizard remains open
    When the user changes the value again and selects OK
    Then the final setting is applied and the wizard closes

  Scenario: ST-2100.3 Cancel uncommitted adjustments
    Given measurement has produced a recommendation
    When the user edits it and selects Cancel without Apply
    Then the previous setting is preserved
    And an unset user value remains unset

  Scenario: ST-2100.4 Retain the dialog safely while removing the filter
    Given measurement is running in the wizard
    When the filter is removed
    Then the open wizard retains the source and filter context
    When the wizard is closed
    Then Qt destroys it and the removed source and context are released
