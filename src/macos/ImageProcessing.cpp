// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ImageProcessing.hpp"

#include <algorithm>
#include <stdexcept>

#include <Accelerate/Accelerate.h>

namespace ImageProcessing {

void thresholdBinaryMask(const float *source, std::uint8_t *destination, std::size_t pixelCount, float threshold)
{
#pragma clang loop vectorize(enable) interleave(enable)
	for (std::size_t index = 0; index < pixelCount; ++index) {
		destination[index] = source[index] > threshold ? std::uint8_t{255} : std::uint8_t{0};
	}
}

void thresholdBinaryMask(std::uint8_t *mask, std::size_t pixelCount, std::uint8_t threshold)
{
#pragma clang loop vectorize(enable)
	for (std::size_t index = 0; index < pixelCount; ++index) {
		mask[index] = mask[index] > threshold ? 255 : 0;
	}
}

void convertFloatMask(const vImage_Buffer &source, const vImage_Buffer &destination)
{
	if (vImageConvert_PlanarFtoPlanar8(&source, &destination, 1.0F, 0.0F, kvImageNoFlags) != kvImageNoError) {
		throw std::runtime_error("vImage float-mask conversion failed");
	}
}

} // namespace ImageProcessing
