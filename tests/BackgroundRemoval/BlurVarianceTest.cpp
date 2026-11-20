// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

// SP-01003: Blur variance

#include <doctest.h>

#include <cstddef>
#include <cstdint>

#include <BackgroundRemoval/MaskCompositor/BlurLevels.hpp>

#include "BlurModel.hpp"

using namespace BackgroundRemoval;
using namespace BackgroundRemoval::BlurModel;

namespace {

// Large enough that the widest blur, strength 20 with a standard deviation of about 52 texels, stays
// clear of the edges.
constexpr std::uint32_t kFrame = 512;
constexpr std::uint32_t kCenter = kFrame / 2;

auto centeredImpulse() -> Image
{
	return impulse(kFrame, kFrame, kCenter, kCenter);
}

} // namespace

TEST_CASE("SP-01003.1 Kawase variance")
{
	struct Example {
		int iterations;
		double variance;
	};
	const Example examples[] = {{1, 0.5}, {4, 22.0}, {8, 172.0}, {20, 2670.0}};
	for (const Example &example : examples) {
		CAPTURE(example.iterations);
		CHECK(kawaseVariance(example.iterations) == doctest::Approx(example.variance).epsilon(1e-12));
		const Moments m = moments(kawase(centeredImpulse(), example.iterations));
		CHECK(m.variance() == doctest::Approx(example.variance).epsilon(1e-9));
	}
}

TEST_CASE("SP-01003.2 Dual filter variance")
{
	for (std::size_t levels = 1; levels <= 6; ++levels) {
		CAPTURE(levels);
		const Moments m = moments(dualFilter(centeredImpulse(), levels));
		CHECK(m.variance() == doctest::Approx(dualVariance(levels)).epsilon(1e-9));
	}
}

TEST_CASE("SP-01003.3 The level weights reproduce the Kawase variance")
{
	struct Example {
		double strength;
		std::size_t levels;
	};
	const Example examples[] = {{0.25, 1}, {1.0, 1}, {2.5, 2}, {4.0, 3}, {8.0, 4}, {20.0, 6}};
	for (const Example &example : examples) {
		CAPTURE(example.strength);
		CHECK(blurLevels(example.strength) == example.levels);
		const Moments m = moments(strengthBlur(centeredImpulse(), example.strength));
		CHECK(m.variance() == doctest::Approx(kawaseVariance(example.strength)).epsilon(1e-9));
	}
}

TEST_CASE("SP-01003.4 Strength 0 does not blur")
{
	CHECK(blurLevels(0.0) == 0);
	const Image frame = centeredImpulse();
	const Image result = strengthBlur(frame, 0.0);
	CHECK(result.texels == frame.texels);
}
