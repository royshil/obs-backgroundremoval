// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

// SP-01001: Output size

#include <doctest.h>

#include <cstdint>

#include <BackgroundRemoval/IRenderingPipeline.hpp>

using BackgroundRemoval::downscaledSize;
using BackgroundRemoval::validOutputDownscaleFactor;

TEST_CASE("SP-01001.1 Downscaled size of one side")
{
	struct Example {
		std::uint32_t size;
		std::int64_t factor;
		std::uint32_t downscaled;
	};
	const Example examples[] = {
		{1280, 1, 1280}, {1280, 2, 640}, {1280, 4, 320}, {1281, 2, 641}, {1281, 4, 321}, {721, 2, 361},
		{721, 4, 181},   {1, 4, 1},      {2, 4, 1},      {3, 4, 1},      {0, 4, 0},
	};
	for (const Example &example : examples) {
		CAPTURE(example.size);
		CAPTURE(example.factor);
		CHECK(downscaledSize(example.size, example.factor) == example.downscaled);
	}
}

TEST_CASE("SP-01001.2 Only 1, 2 and 4 are valid downscale factors")
{
	struct Example {
		std::int64_t stored;
		std::int64_t factor;
	};
	const Example examples[] = {{1, 1}, {2, 2}, {4, 4}, {0, 1}, {3, 1}, {8, 1}, {-2, 1}};
	for (const Example &example : examples) {
		CAPTURE(example.stored);
		CHECK(validOutputDownscaleFactor(example.stored) == example.factor);
	}
}
