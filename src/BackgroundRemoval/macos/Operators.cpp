// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "BackgroundRemoval/Operators.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <stdexcept>

#include <Accelerate/Accelerate.h>

namespace Operators {

namespace {

constexpr std::uint32_t bufferAlignment = 32;

auto isValid(const void *data, ImageLayout layout, std::uint32_t bytesPerElement) noexcept -> bool
{
	return data && layout.width > 0 && layout.height > 0 && layout.linesize >= layout.width * bytesPerElement;
}

} // namespace

auto PsnrF32::calculate(const float *first, ImageLayout firstLayout, const float *second,
			ImageLayout secondLayout) const -> double
{
	if (!isValid(first, firstLayout, sizeof(float)) || !isValid(second, secondLayout, sizeof(float)) ||
	    firstLayout.width != secondLayout.width || firstLayout.height != secondLayout.height) {
		return std::numeric_limits<double>::quiet_NaN();
	}
	double squaredError = 0.0;
	for (std::uint32_t row = 0; row < firstLayout.height; ++row) {
		const auto *firstRow =
			reinterpret_cast<const float *>(reinterpret_cast<const std::uint8_t *>(first) +
							static_cast<std::size_t>(row) * firstLayout.linesize);
		const auto *secondRow =
			reinterpret_cast<const float *>(reinterpret_cast<const std::uint8_t *>(second) +
							static_cast<std::size_t>(row) * secondLayout.linesize);
#pragma clang loop vectorize(enable) interleave(enable)
		for (std::uint32_t column = 0; column < firstLayout.width; ++column) {
			const double difference = static_cast<double>(firstRow[column]) - secondRow[column];
			squaredError += difference * difference;
		}
	}
	if (squaredError == 0.0) {
		return std::numeric_limits<double>::infinity();
	}
	return 10.0 * std::log10(static_cast<double>(firstLayout.width) * firstLayout.height / squaredError);
}

auto ResizeU8::apply(const std::uint8_t *source, ImageLayout sourceLayout, std::uint8_t *destination,
		     ImageLayout destinationLayout) -> bool
{
	if (!isValid(source, sourceLayout, 1) || !isValid(destination, destinationLayout, 1)) {
		return false;
	}
	vImage_Buffer input{const_cast<std::uint8_t *>(source), sourceLayout.height, sourceLayout.width,
			    sourceLayout.linesize};
	vImage_Buffer output{destination, destinationLayout.height, destinationLayout.width,
			     destinationLayout.linesize};
	const vImage_Error temporarySize =
		vImageScale_Planar8(&input, &output, nullptr, kvImageGetTempBufferSize | kvImageHighQualityResampling);
	if (temporarySize < kvImageNoError) {
		return false;
	}
	temporaryBuffer_.resize(static_cast<std::size_t>(temporarySize));
	void *temporaryData = temporaryBuffer_.empty() ? nullptr : temporaryBuffer_.data();
	return vImageScale_Planar8(&input, &output, temporaryData, kvImageHighQualityResampling) == kvImageNoError;
}

SmoothU8::SmoothU8(ImageSpec spec) : spec_(spec)
{
	if (spec.width == 0 || spec.height == 0) {
		throw std::invalid_argument("SmoothU8 requires a non-empty image spec");
	}
	vImage_Buffer source{nullptr, spec.height, spec.width, spec.width};
	vImage_Buffer destination{nullptr, spec.height, spec.width, spec.width};
	const vImage_Error temporarySize = vImageTentConvolve_Planar8(&source, &destination, nullptr, 0, 0, 9, 9, 0,
								      kvImageGetTempBufferSize | kvImageEdgeExtend);
	if (temporarySize < kvImageNoError) {
		throw std::runtime_error("Failed to determine SmoothU8 temporary storage size");
	}
	if (temporarySize > 0) {
		temporaryBuffer_ = Memory::makeAlignedBytes(static_cast<std::size_t>(temporarySize), bufferAlignment);
	}
}

auto SmoothU8::apply(const std::uint8_t *source, std::uint32_t sourceLinesize, std::uint8_t *destination,
		     std::uint32_t destinationLinesize) -> bool
{
	if (!isValid(source, {spec_.width, spec_.height, sourceLinesize}, 1) ||
	    !isValid(destination, {spec_.width, spec_.height, destinationLinesize}, 1)) {
		return false;
	}
	vImage_Buffer input{const_cast<std::uint8_t *>(source), spec_.height, spec_.width, sourceLinesize};
	vImage_Buffer output{destination, spec_.height, spec_.width, destinationLinesize};
	return vImageTentConvolve_Planar8(&input, &output, temporaryBuffer_.get(), 0, 0, 9, 9, 0, kvImageEdgeExtend) ==
	       kvImageNoError;
}

MorphologyU8::MorphologyU8(ImageSpec spec) : spec_(spec), internalLinesize_(0)
{
	if (spec.width == 0 || spec.height == 0 ||
	    spec.width > std::numeric_limits<std::uint32_t>::max() - (bufferAlignment - 1)) {
		throw std::invalid_argument("MorphologyU8 requires a non-empty image spec");
	}
	internalLinesize_ = (spec.width + bufferAlignment - 1) & ~(bufferAlignment - 1);
	const std::size_t bufferSize = static_cast<std::size_t>(internalLinesize_) * spec.height;
	firstBuffer_ = Memory::makeAlignedBytes(bufferSize, bufferAlignment);
	secondBuffer_ = Memory::makeAlignedBytes(bufferSize, bufferAlignment);
	vImage_Buffer input{firstBuffer_.get(), spec.height, spec.width, internalLinesize_};
	vImage_Buffer output{secondBuffer_.get(), spec.height, spec.width, internalLinesize_};
	const vImage_Error erodeTemporarySize =
		vImageMin_Planar8(&input, &output, nullptr, 0, 0, 3, 3, kvImageGetTempBufferSize);
	const vImage_Error dilateTemporarySize =
		vImageMax_Planar8(&input, &output, nullptr, 0, 0, 3, 3, kvImageGetTempBufferSize);
	if (erodeTemporarySize < kvImageNoError || dilateTemporarySize < kvImageNoError) {
		throw std::runtime_error("Failed to determine MorphologyU8 temporary storage size");
	}
	const auto temporarySize = static_cast<std::size_t>(std::max(erodeTemporarySize, dilateTemporarySize));
	if (temporarySize > 0) {
		temporaryBuffer_ = Memory::makeAlignedBytes(temporarySize, bufferAlignment);
	}
}

auto MorphologyU8::erode() -> bool
{
	vImage_Buffer input{firstBuffer_.get(), spec_.height, spec_.width, internalLinesize_};
	vImage_Buffer output{secondBuffer_.get(), spec_.height, spec_.width, internalLinesize_};
	void *temporaryData = temporaryBuffer_.get();
	if (vImageMin_Planar8(&input, &output, temporaryData, 0, 0, 3, 3, kvImageEdgeExtend) != kvImageNoError) {
		return false;
	}
	firstBuffer_.swap(secondBuffer_);
	return true;
}

auto MorphologyU8::dilate() -> bool
{
	vImage_Buffer input{firstBuffer_.get(), spec_.height, spec_.width, internalLinesize_};
	vImage_Buffer output{secondBuffer_.get(), spec_.height, spec_.width, internalLinesize_};
	void *temporaryData = temporaryBuffer_.get();
	if (vImageMax_Planar8(&input, &output, temporaryData, 0, 0, 3, 3, kvImageEdgeExtend) != kvImageNoError) {
		return false;
	}
	firstBuffer_.swap(secondBuffer_);
	return true;
}

} // namespace Operators
