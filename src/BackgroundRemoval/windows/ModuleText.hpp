// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace BackgroundRemoval {

auto tBackgroundRemoval() -> const char *;
auto tUpToDate() -> const char *;
auto tNewVersionsAvailable() -> const char *;
auto tGeneral() -> const char *;
auto tMaskAdjustment() -> const char *;
auto tMaskAdjustmentDescription() -> const char *;
auto tMotionSuppression() -> const char *;
auto tMotionSuppressionDescription() -> const char *;
auto tBlurBg() -> const char *;
auto tBlurBgStrength() -> const char *;
auto tBlurBgFocusPoint() -> const char *;
auto tBlurBgFocusDepth() -> const char *;
auto tAboutThisPlugin() -> const char *;

} // namespace BackgroundRemoval
