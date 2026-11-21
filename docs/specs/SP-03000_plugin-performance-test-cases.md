---
# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0
---

# SP-03000: Plugin performance test cases

Draft. The shape of this category is not settled yet.

## Purpose

Performance test cases pin down how fast the plugin must render for OBS Studio to keep its frame
rate: that changing settings or adding filters does not stall the video thread of libobs, and other
limits on time.

Behavior that a program can check without regard to speed, such as settings, filter chains, sizes,
rendered output, and log lines, belongs to the unit test cases of SP-01000, not here.

## How they are tested

The cases are tested with doctest, the same way as the unit test cases: libobs is started headless,
and the plugin and the OBS Studio modules the cases use are loaded. A test names the case it
implements by its tag, such as `SP-03001.1`, so that each scenario maps to a test case.

The cases need a hardware GPU. CI runners have none and render in software, which cannot keep the
frame rate, so CI does not run these cases; they are run on a machine with a hardware GPU.

## What to write

- Cases as Gherkin, as `SP-03xxx_case-title.feature`, one feature per unit or topic.
- Start the case title in the file name with an abbreviation of what the case covers, such as
  `bgrm` for the Background Removal filter (`SP-03001_bgrm-rendering-does-not-stall.feature`). The
  abbreviations are used in spec file names only.
- Tag the feature with its number (`@SP-03xxx`) and each scenario with a sub-number
  (`@SP-03xxx.1`).
- Limits stated exactly, such as a number of skipped frames, so that a doctest assertion can check
  them. Settings are named by their keys, such as `"v2_OutputDownscaleFactor"`.
- The design principle a case verifies, if any.

The cases hold on all platforms with a hardware GPU; platform-specific ways to run them belong to
the tests, not here.
