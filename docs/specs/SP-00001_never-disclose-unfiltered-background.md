---
# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0
---

# SP-00001: Never disclose the unfiltered background

- Kind: Design principle
- Platforms: All

## Principle

The filter never draws its input unprocessed. Whenever it cannot produce a filtered frame, it draws
nothing.

## Rationale

Users add the filter to hide what is behind them. Falling back to the unfiltered frame, as OBS
filters usually do with `obs_source_skip_video_filter()`, would show exactly what the user meant to
hide, at the moment they are least likely to notice.

## Requirements

1. The filter draws nothing while its parent source is not showing.
2. The filter draws nothing while it has no rendering pipeline, for example before the first one
   is created, after creation failed, or while the selected pipeline is unknown.
3. A rendering pipeline draws nothing after a GPU failure, and while the size of the filter's target
   differs from the size it was created for.
4. While the filter has no rendering pipeline, it reports an output size of 0 x 0.
5. When the filter is disabled, OBS bypasses it; this principle does not apply.

## Verified by

- SP-01014: Unknown pipeline
