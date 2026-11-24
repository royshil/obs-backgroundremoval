---
# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0
---

# SP-00002: Pipeline lifecycle

- Kind: Design principle
- Platforms: All

## Principle

A rendering pipeline is created for one fixed combination of the selected pipeline and the size of
the filter's target, and is recreated whenever either of them changes. The filter reports the
output size of the pipeline it has, which is the size of the target.

## Rationale

Fixing the sizes at creation keeps every GPU resource of a pipeline the right size for its whole
life, and makes the output size the filter reports always match what it draws. OBS's own
size-changing filters, Scaling/Aspect Ratio and Crop/Pad, follow the same pattern: they compute the
output size in `video_tick` from the target's base size, and `get_width`/`get_height` return it.

## Requirements

1. The filter decides in `video_tick` whether to recreate the pipeline, and creates or destroys it
    inside the graphics context.
2. The pipeline is recreated when the pipeline selection or the target's size changes. A
    combination that failed to create is not retried until one of them changes again.
3. The target's size is its base size, `obs_source_get_base_width/height(target)`. It reflects
    any size-changing filter before this one. `obs_source_get_width/height` of the parent source
    would return the size of its first filter, possibly this filter, under the parent's filter
    mutex.
4. `get_width` and `get_height` return the output size of the current pipeline, or 0 without one,
    and query no source size.
5. The filter creates no pipeline while its parent is not showing. A disabled filter keeps its
    pipeline as a hot standby and keeps it current: users toggle filters quickly, and enabling one
    must draw at once without recreating anything.
6. Effects are compiled once and shared by all pipelines and filters. `gs_effect_create` with a
    name never frees the effect and never reuses one, so a pipeline does not create its own.
7. The frame is processed and drawn at the target's size. There is no setting that makes the
    output smaller: a source shown small is scaled by OBS after the filter.

## Verified by

- SP-01006: Output size
- SP-01007: Odd source size
- SP-01008: Preceding scale filter
- SP-01009: Preceding crop filter
- SP-01010: Following filter
- SP-01011: Target resized at runtime
- SP-01012: Disabled filter
- SP-01013: Shown outside the program output
- SP-01015: Repeated pipeline selection changes
- SP-02001: Preview only
- SP-03001: Rendering does not stall
