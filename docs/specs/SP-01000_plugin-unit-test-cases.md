---
# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0
---

# SP-01000: Plugin unit test cases

Draft. The shape of this category is not settled yet.

## Purpose

Unit test cases pin down the behavior of the plugin that a program can check without the OBS
Studio frontend: computations, data conversions, components driven through libobs graphics, and
the filter itself running in libobs, with sources, filter chains, settings, and rendered output.

Linking libobs does not make a case anything other than a unit test case. The plugin cannot be
separated from OBS, so a category of cases without libobs would carry no meaning.

## How they are tested

The cases are tested with doctest. Cases that need libobs start it headless and load the plugin and
the OBS Studio modules they use. A test names the case it implements by its tag, such as
`SP-01004.1`, so that each scenario maps to a test case.

## What to write

- Cases as Gherkin, as `SP-01xxx_case-title.feature`, one feature per unit or topic.
- Start the case title in the file name with an abbreviation of what the case covers, such as
  `bgrm` for the Background Removal filter (`SP-01004_bgrm-mask-refinement.feature`). The
  abbreviations are used in spec file names only.
- Tag the feature with its number (`@SP-01xxx`) and each scenario with a sub-number
  (`@SP-01xxx.1`).
- Inputs and expected outputs stated exactly, with tolerances where values are numeric, so that a
  doctest assertion can check them. Settings are named by their keys, such as
  `"v2_BlurBg_FocusRatio"`, and log lines are quoted in full.
- The design principle a case verifies, if any.

The cases hold on all platforms; platform-specific ways to run them belong to the tests, not here.
