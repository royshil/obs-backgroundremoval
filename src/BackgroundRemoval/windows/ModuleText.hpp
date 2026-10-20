// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace BackgroundRemoval {

auto tBackgroundRemoval() -> const char *;
auto tUpToDate() -> const char *;
auto tNewVersionsAvailable() -> const char *;
auto tPipeline() -> const char *;
auto tPipelineAuto() -> const char *;
auto tPipelineMPLD10() -> const char *;
auto tPipelineDescription() -> const char *;
auto tMaskAdjustment() -> const char *;
auto tMaskAdjustmentDescription() -> const char *;
auto tMotionSuppression() -> const char *;
auto tMotionSuppressionDescription() -> const char *;
auto tAutoSetSensitivity() -> const char *;
auto tAutoSetSensitivityDescription() -> const char *;
auto tAutoMotionTitle() -> const char *;
auto tAutoMotionInstructions() -> const char *;
auto tStartMotionMeasurement() -> const char *;
auto tMeasureMotionAgain() -> const char *;
auto tMeasuringMotion() -> const char *;
auto tNoMotionInput() -> const char *;
auto tMotionMeasurementResult() -> const char *;
auto tBlurBg() -> const char *;
auto tBlurBgStrength() -> const char *;
auto tBlurBgFocusPoint() -> const char *;
auto tBlurBgFocusDepth() -> const char *;
auto tRefineMask() -> const char *;

} // namespace BackgroundRemoval
