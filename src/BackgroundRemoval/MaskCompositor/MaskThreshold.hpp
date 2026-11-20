// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <cmath>

namespace BackgroundRemoval {

/// The foreground probability threshold for a mask adjustment degree in [-50, 50]: 0.5 - degree /
/// 100, so positive degrees keep more foreground. Degrees outside the range are clamped, and a
/// non-finite degree is 0.
inline auto maskThreshold(double degree) noexcept -> float
{
	const double adjustment = std::isfinite(degree) ? std::clamp(degree, -50.0, 50.0) : 0.0;
	return static_cast<float>(0.5 - adjustment / 100.0);
}

} // namespace BackgroundRemoval
