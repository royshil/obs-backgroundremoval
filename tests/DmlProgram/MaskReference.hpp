// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

namespace BackgroundRemoval::PipelineTest {

// Independent model-sized threshold and Box reference.
inline auto coarseMask(std::span<const float> probabilities, float threshold = 0.5F) -> std::vector<float>
{
	std::vector<float> values(256 * 144);
	for (int y = 0; y < 144; ++y)
		for (int x = 0; x < 256; ++x) {
			std::uint32_t sum = 0, count = 0;
			for (int yy = std::max(0, y - 2); yy <= std::min(143, y + 2); ++yy)
				for (int xx = std::max(0, x - 2); xx <= std::min(255, x + 2); ++xx) {
					sum += probabilities[yy * 256 + xx] < threshold;
					++count;
				}
			values[y * 256 + x] = float(sum) / float(count);
		}

	return values;
}

// Reference for the final GS linear sample and strict binary comparison.
inline auto refinedMask(std::span<const float> probabilities, std::uint32_t width = 256, std::uint32_t height = 144,
			float threshold = 0.5F) -> std::vector<std::uint8_t>
{
	const auto values = coarseMask(probabilities, threshold);
	auto at = [&](int x, int y) {
		return float(values[std::clamp(y, 0, 143) * 256 + std::clamp(x, 0, 255)]);
	};
	std::vector<std::uint8_t> output(std::size_t(width) * height);
	const float scaleX = 256.0F / static_cast<float>(width);
	const float scaleY = 144.0F / static_cast<float>(height);
	for (std::uint32_t y = 0; y < height; ++y)
		for (std::uint32_t x = 0; x < width; ++x) {
			const auto sx = (float(x) + 0.5F) * scaleX - 0.5F;
			const auto sy = (float(y) + 0.5F) * scaleY - 0.5F;
			const auto ix = int(std::floor(sx)), iy = int(std::floor(sy));
			const auto fx = sx - ix, fy = sy - iy;
			const auto top = at(ix, iy) + (at(ix + 1, iy) - at(ix, iy)) * fx;
			const auto bottom = at(ix, iy + 1) + (at(ix + 1, iy + 1) - at(ix, iy + 1)) * fx;
			output[y * width + x] = (top + (bottom - top) * fy) > 0.5F ? 255 : 0;
		}
	return output;
}

} // namespace BackgroundRemoval::PipelineTest
