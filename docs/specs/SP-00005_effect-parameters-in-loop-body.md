---
# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0
---

# SP-00005: Effect parameters in the loop body

- Kind: Design principle
- Platforms: All

## Principle

Every draw goes through `gs_effect_loop()` and sets all the parameters it reads inside the loop
body, right before the draw.

## Rationale

`gs_technique_end()` clears the values of every parameter of the effect when the loop ends, so a
value set before an earlier loop is gone by the next one. Setting the parameters in the body ties
each value to the draw it is for. The draw uploads changed parameters, so this works on every
graphics backend.

## Requirements

1. No code calls `gs_technique_begin`, `gs_technique_begin_pass` or `gs_technique_end` directly.
2. Each loop body sets every parameter its technique reads, including null textures for texture
   parameters it does not use, then draws.
3. Techniques are looked up by name. Every name is checked once when the effect is first used,
   because `gs_effect_loop()` draws nothing for a missing technique.
