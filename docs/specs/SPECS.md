---
# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0
---

# Index of specs

## Design principles

- [SP-00000: Design principles](SP-00000_design-principles.md)
- [SP-00001: Never disclose the unfiltered background](SP-00001_never-disclose-unfiltered-background.md)
- [SP-00002: Pipeline lifecycle](SP-00002_pipeline-lifecycle.md)
- [SP-00003: Published techniques, as published](SP-00003_published-techniques-as-published.md)
- [SP-00004: Focal blur model](SP-00004_focal-blur-model.md)
- [SP-00005: Effect parameters in the loop body](SP-00005_effect-parameters-in-loop-body.md)
- [SP-00006: Vendored files record their upstream](SP-00006_vendored-files-record-their-upstream.md)

## Plugin unit test cases

- [SP-01000: Plugin unit test cases](SP-01000_plugin-unit-test-cases.md)
- [SP-01001: Output size](SP-01001_bgrm-output-size.feature)
- [SP-01002: Dual filter kernel](SP-01002_bgrm-dual-filter-kernel.feature)
- [SP-01003: Blur variance](SP-01003_bgrm-blur-variance.feature)
- [SP-01004: Mask refinement](SP-01004_bgrm-mask-refinement.feature)
- [SP-01005: MediaPipe Selfie Segmenter inference](SP-01005_bgrm-mpls-inference.feature)
- [SP-01006: Output downscale factor](SP-01006_bgrm-output-downscale-factor.feature)
- [SP-01007: Odd source size](SP-01007_bgrm-odd-source-size.feature)
- [SP-01008: Preceding scale filter](SP-01008_bgrm-preceding-scale-filter.feature)
- [SP-01009: Preceding crop filter](SP-01009_bgrm-preceding-crop-filter.feature)
- [SP-01010: Following filter](SP-01010_bgrm-following-filter.feature)
- [SP-01011: Target resized at runtime](SP-01011_bgrm-target-resized-at-runtime.feature)
- [SP-01012: Disabled filter](SP-01012_bgrm-disabled-filter.feature)
- [SP-01013: Shown outside the program output](SP-01013_bgrm-shown-outside-program.feature)
- [SP-01014: Unknown pipeline](SP-01014_bgrm-unknown-pipeline.feature)
- [SP-01015: Repeated downscale factor changes](SP-01015_bgrm-repeated-downscale-factor-changes.feature)
- [SP-01016: Blur strength](SP-01016_bgrm-blur-strength.feature)
- [SP-01017: Blur focus ratio](SP-01017_bgrm-blur-focus-ratio.feature)
- [SP-01018: Kept pixels](SP-01018_bgrm-kept-pixels.feature)
- [SP-01019: Settings compatibility](SP-01019_bgrm-settings-compatibility.feature)

## Plugin user scenario cases

- [SP-02000: Plugin user scenario cases](SP-02000_plugin-user-scenario-cases.md)
- [SP-02001: Preview only](SP-02001_bgrm-preview-only.feature)

## Plugin performance test cases

- [SP-03000: Plugin performance test cases](SP-03000_plugin-performance-test-cases.md)
- [SP-03001: Rendering does not stall](SP-03001_bgrm-rendering-does-not-stall.feature)
