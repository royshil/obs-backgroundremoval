// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Operators.hpp"

#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>

#if defined(__AVX2__)
#include <immintrin.h>
#endif
#include <opencv2/imgproc.hpp>

namespace Operators {

namespace {

constexpr std::uint32_t bufferAlignment = 32;

auto isValid(const void *data, ImageLayout layout, std::uint32_t bytesPerElement) noexcept -> bool
{
	return data && layout.width > 0 && layout.height > 0 && layout.linesize >= layout.width * bytesPerElement;
}

auto asGrayMat(const std::uint8_t *data, ImageLayout layout) -> cv::Mat
{
	return {static_cast<int>(layout.height), static_cast<int>(layout.width), CV_8UC1,
		const_cast<std::uint8_t *>(data), layout.linesize};
}

auto asGrayMat(std::uint8_t *data, ImageLayout layout) -> cv::Mat
{
	return {static_cast<int>(layout.height), static_cast<int>(layout.width), CV_8UC1, data, layout.linesize};
}

} // namespace

auto PsnrF32::calculate(const float *first, ImageLayout firstLayout, const float *second,
			ImageLayout secondLayout) const -> double
{
	if (!isValid(first, firstLayout, sizeof(float)) || !isValid(second, secondLayout, sizeof(float)) ||
	    firstLayout.width != secondLayout.width || firstLayout.height != secondLayout.height) {
		return std::numeric_limits<double>::quiet_NaN();
	}

	double scalarSum = 0.0;
#if defined(__AVX2__)
	__m256d sumsLow = _mm256_setzero_pd();
	__m256d sumsHigh = _mm256_setzero_pd();
#endif
	for (std::uint32_t row = 0; row < firstLayout.height; ++row) {
		const auto *firstRow =
			reinterpret_cast<const float *>(reinterpret_cast<const std::uint8_t *>(first) +
							static_cast<std::size_t>(row) * firstLayout.linesize);
		const auto *secondRow =
			reinterpret_cast<const float *>(reinterpret_cast<const std::uint8_t *>(second) +
							static_cast<std::size_t>(row) * secondLayout.linesize);
		std::uint32_t column = 0;
#if defined(__AVX2__)
		for (; column + 8 <= firstLayout.width; column += 8) {
			const __m256 difference =
				_mm256_sub_ps(_mm256_loadu_ps(firstRow + column), _mm256_loadu_ps(secondRow + column));
			const __m256 squared = _mm256_mul_ps(difference, difference);
			sumsLow = _mm256_add_pd(sumsLow, _mm256_cvtps_pd(_mm256_castps256_ps128(squared)));
			sumsHigh = _mm256_add_pd(sumsHigh, _mm256_cvtps_pd(_mm256_extractf128_ps(squared, 1)));
		}
#endif
		for (; column < firstLayout.width; ++column) {
			const double difference = static_cast<double>(firstRow[column]) - secondRow[column];
			scalarSum += difference * difference;
		}
	}

#if defined(__AVX2__)
	alignas(32) double vectorSums[4];
	_mm256_store_pd(vectorSums, _mm256_add_pd(sumsLow, sumsHigh));
	const double squaredError = scalarSum + vectorSums[0] + vectorSums[1] + vectorSums[2] + vectorSums[3];
#else
	const double squaredError = scalarSum;
#endif
	if (squaredError == 0.0) {
		return std::numeric_limits<double>::infinity();
	}
	const double elementCount = static_cast<double>(firstLayout.width) * firstLayout.height;
	return 10.0 * std::log10(elementCount / squaredError);
}

auto ResizeU8::apply(const std::uint8_t *source, ImageLayout sourceLayout, std::uint8_t *destination,
		     ImageLayout destinationLayout) -> bool
{
	if (!isValid(source, sourceLayout, 1) || !isValid(destination, destinationLayout, 1)) {
		return false;
	}
	cv::resize(asGrayMat(source, sourceLayout), asGrayMat(destination, destinationLayout),
		   cv::Size(destinationLayout.width, destinationLayout.height), 0.0, 0.0, cv::INTER_LINEAR);
	return true;
}

SmoothU8::SmoothU8(ImageSpec spec) : spec_(spec)
{
	if (spec.width == 0 || spec.height == 0) {
		throw std::invalid_argument("SmoothU8 requires a non-empty image spec");
	}
}

auto SmoothU8::apply(const std::uint8_t *source, std::uint32_t sourceLinesize, std::uint8_t *destination,
		     std::uint32_t destinationLinesize) -> bool
{
	if (!isValid(source, {spec_.width, spec_.height, sourceLinesize}, 1) ||
	    !isValid(destination, {spec_.width, spec_.height, destinationLinesize}, 1)) {
		return false;
	}
	cv::stackBlur(asGrayMat(source, {spec_.width, spec_.height, sourceLinesize}),
		      asGrayMat(destination, {spec_.width, spec_.height, destinationLinesize}), cv::Size(9, 9));
	return true;
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
}

auto MorphologyU8::erode() -> bool
{
	const ImageLayout layout{spec_.width, spec_.height, internalLinesize_};
	cv::erode(asGrayMat(firstBuffer_.get(), layout), asGrayMat(secondBuffer_.get(), layout), cv::Mat());
	firstBuffer_.swap(secondBuffer_);
	return true;
}

auto MorphologyU8::dilate() -> bool
{
	const ImageLayout layout{spec_.width, spec_.height, internalLinesize_};
	cv::dilate(asGrayMat(firstBuffer_.get(), layout), asGrayMat(secondBuffer_.get(), layout), cv::Mat());
	firstBuffer_.swap(secondBuffer_);
	return true;
}

} // namespace Operators
