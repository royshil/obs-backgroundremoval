---
# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0
---

# SP-02000: Plugin user scenario cases

Draft. The shape of this category is not settled yet.

## Purpose

User scenario cases describe what a user does with the plugin in the OBS Studio frontend and what
they must see there: the properties dialog, the previews, and other parts of the interface.

Behavior that a program can check through libobs, such as settings, filter chains, sizes, and
rendered output, belongs to the unit test cases of SP-01000, not here.

## How they are tested

The cases are tested by driving OBS Studio through the platform's accessibility API, as a user
would: finding and operating windows, dialogs, and controls by their accessible names and roles.
A test names the case it implements by its tag, such as `SP-02001.1`.

## What to write

- Cases as Gherkin, as `SP-02xxx_case-title.feature`, one feature per user-facing topic.
- Start the case title in the file name with an abbreviation of what the case covers, such as
  `bgrm` for the Background Removal filter (`SP-02001_bgrm-preview-only.feature`). The
  abbreviations are used in spec file names only.
- Tag the feature with its number (`@SP-02xxx`) and each scenario with a sub-number
  (`@SP-02xxx.1`).
- The scene and source setup in `Background`, the user's actions in `When`, and observable results
  in `Then`: what is shown, its size, and what OBS logs.
- Actions as a user performs them in the OBS Studio interface, through the names they see, such as
  the Focus ratio setting, so that the test can find them through the accessibility API.
- Results that a test can check without judgment, such as sizes, pixel agreement with a threshold,
  or exact log lines.

The cases hold on all platforms; platform-specific ways to run them belong to the tests, not here.
