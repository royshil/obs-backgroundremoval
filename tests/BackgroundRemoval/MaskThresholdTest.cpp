// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

// SP-01004: Mask refinement (the threshold)

#include <doctest.h>

#include <BackgroundRemoval/MaskCompositor/MaskThreshold.hpp>

using BackgroundRemoval::maskThreshold;

TEST_CASE("SP-01004.3 Mask adjustment moves the threshold")
{
	struct Example {
		double adjustment;
		float threshold;
	};
	const Example examples[] = {{0.0, 0.5F}, {30.0, 0.2F}, {-30.0, 0.8F}, {80.0, 0.0F}, {-80.0, 1.0F}};
	for (const Example &example : examples) {
		CAPTURE(example.adjustment);
		CHECK(maskThreshold(example.adjustment) == doctest::Approx(example.threshold).epsilon(1e-6));
	}
}
