---
# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0
---

# SP-00000: Design principles

Draft. The shape of this category is not settled yet.

## Purpose

Design principles record decisions that hold across features and platforms, and the reasons for
them, so that later changes can be checked against them instead of rediscovering them.

## What to write

- One principle per document, as `SP-00xxx_topic-title.md`.
- The front matter gives its `kind`, `Design principle`, and the `platforms` it applies to.
- The principle itself, in a sentence or two.
- Why it holds: the user-facing reason or the platform constraint behind it.
- Concrete requirements that follow from it, specific enough to test.
- The cases (`SP-01xxx`, `SP-02xxx`) that verify it.

Keep implementation details out unless they are the constraint itself, such as an OBS API whose
behavior forces the design.
