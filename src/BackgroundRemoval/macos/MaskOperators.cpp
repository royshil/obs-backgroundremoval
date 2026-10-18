// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Operators.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace Operators {

namespace {

auto isValid(const std::uint8_t *data, ImageLayout layout) noexcept -> bool
{
	return data && layout.width > 0 && layout.height > 0 && layout.linesize >= layout.width;
}

} // namespace

ThresholdU8::ThresholdU8(ImageSpec spec) : spec_(spec)
{
	if (spec.width == 0 || spec.height == 0) {
		throw std::invalid_argument("ThresholdU8 requires a non-empty image spec");
	}
}

auto ThresholdU8::apply(const std::uint8_t *source, std::uint32_t sourceLinesize, std::uint8_t *destination,
			std::uint32_t destinationLinesize, std::uint8_t threshold) const noexcept -> bool
{
	if (!isValid(source, {spec_.width, spec_.height, sourceLinesize}) ||
	    !isValid(destination, {spec_.width, spec_.height, destinationLinesize})) {
		return false;
	}
	for (std::uint32_t row = 0; row < spec_.height; ++row) {
		const auto *sourceRow = source + static_cast<std::size_t>(row) * sourceLinesize;
		auto *destinationRow = destination + static_cast<std::size_t>(row) * destinationLinesize;
		for (std::uint32_t column = 0; column < spec_.width; ++column) {
			destinationRow[column] = sourceRow[column] > threshold ? 255 : 0;
		}
	}
	return true;
}

ThresholdF32::ThresholdF32(ImageSpec spec) : spec_(spec)
{
	if (spec.width == 0 || spec.height == 0) {
		throw std::invalid_argument("ThresholdF32 requires a non-empty image spec");
	}
}

auto ThresholdF32::apply(const float *source, std::uint32_t sourceLinesize, std::uint8_t *destination,
			 std::uint32_t destinationLinesize, float threshold) const noexcept -> bool
{
	if (!source || !isValid(destination, {spec_.width, spec_.height, destinationLinesize}) ||
	    sourceLinesize < spec_.width * sizeof(float)) {
		return false;
	}
	for (std::uint32_t row = 0; row < spec_.height; ++row) {
		const auto *sourceRow = reinterpret_cast<const float *>(reinterpret_cast<const std::uint8_t *>(source) +
									static_cast<std::size_t>(row) * sourceLinesize);
		auto *destinationRow = destination + static_cast<std::size_t>(row) * destinationLinesize;
		for (std::uint32_t column = 0; column < spec_.width; ++column) {
			destinationRow[column] = sourceRow[column] > threshold ? 255 : 0;
		}
	}
	return true;
}

auto MorphologyU8::begin(const std::uint8_t *source, std::uint32_t sourceLinesize) -> bool
{
	if (!isValid(source, {spec_.width, spec_.height, sourceLinesize})) {
		return false;
	}
	for (std::uint32_t row = 0; row < spec_.height; ++row) {
		std::memcpy(firstBuffer_.get() + static_cast<std::size_t>(row) * internalLinesize_,
			    source + static_cast<std::size_t>(row) * sourceLinesize, spec_.width);
	}
	return true;
}

auto MorphologyU8::end(std::uint8_t *destination, std::uint32_t destinationLinesize) -> bool
{
	if (!isValid(destination, {spec_.width, spec_.height, destinationLinesize})) {
		return false;
	}
	for (std::uint32_t row = 0; row < spec_.height; ++row) {
		std::memcpy(destination + static_cast<std::size_t>(row) * destinationLinesize,
			    firstBuffer_.get() + static_cast<std::size_t>(row) * internalLinesize_, spec_.width);
	}
	return true;
}

// Connected-component labeling follows the sequential labeling approach described in:
// A. Rosenfeld and J. L. Pfaltz, "Sequential Operations in Digital Picture Processing,"
// Journal of the ACM 13(4), 471-494, 1966. https://doi.org/10.1145/321356.321357
auto RemoveSmallComponentsU8::apply(const std::uint8_t *source, ImageLayout sourceLayout, std::uint8_t *destination,
				    ImageLayout destinationLayout, double minimumArea) -> bool
{
	if (!isValid(source, sourceLayout) || !isValid(destination, destinationLayout) ||
	    sourceLayout.width != destinationLayout.width || sourceLayout.height != destinationLayout.height ||
	    minimumArea < 0.0 || sourceLayout.width > std::numeric_limits<std::uint32_t>::max() / sourceLayout.height) {
		return false;
	}
	const std::size_t pixelCount = static_cast<std::size_t>(sourceLayout.width) * sourceLayout.height;
	labelBuffer_.assign(pixelCount, 0);
	labelTable_.assign(pixelCount + 1, 0);
	auto findRoot = [this](std::uint32_t label) {
		std::uint32_t root = label;
		while (labelTable_[root] != root) {
			root = labelTable_[root];
		}
		while (labelTable_[label] != label) {
			const std::uint32_t parent = labelTable_[label];
			labelTable_[label] = root;
			label = parent;
		}
		return root;
	};
	std::uint32_t nextLabel = 1;
	for (std::uint32_t y = 0; y < sourceLayout.height; ++y) {
		const auto *sourceRow = source + static_cast<std::size_t>(y) * sourceLayout.linesize;
		for (std::uint32_t x = 0; x < sourceLayout.width; ++x) {
			const std::size_t index = static_cast<std::size_t>(y) * sourceLayout.width + x;
			if (sourceRow[x] == 0) {
				continue;
			}
			std::uint32_t neighbors[4]{};
			std::size_t neighborCount = 0;
			if (x > 0 && labelBuffer_[index - 1] != 0) {
				neighbors[neighborCount++] = labelBuffer_[index - 1];
			}
			if (y > 0) {
				const std::size_t previous = index - sourceLayout.width;
				if (x > 0 && labelBuffer_[previous - 1] != 0)
					neighbors[neighborCount++] = labelBuffer_[previous - 1];
				if (labelBuffer_[previous] != 0)
					neighbors[neighborCount++] = labelBuffer_[previous];
				if (x + 1 < sourceLayout.width && labelBuffer_[previous + 1] != 0)
					neighbors[neighborCount++] = labelBuffer_[previous + 1];
			}
			if (neighborCount == 0) {
				labelBuffer_[index] = nextLabel;
				labelTable_[nextLabel] = nextLabel;
				++nextLabel;
				continue;
			}
			const std::uint32_t label = *std::min_element(neighbors, neighbors + neighborCount);
			labelBuffer_[index] = label;
			for (std::size_t neighbor = 0; neighbor < neighborCount; ++neighbor) {
				const std::uint32_t first = findRoot(label);
				const std::uint32_t second = findRoot(neighbors[neighbor]);
				if (first != second)
					labelTable_[std::max(first, second)] = std::min(first, second);
			}
		}
	}
	for (std::uint32_t label = 1; label < nextLabel; ++label)
		labelTable_[label] = findRoot(label);
	for (std::size_t index = 0; index < pixelCount; ++index) {
		if (labelBuffer_[index] != 0)
			labelBuffer_[index] = labelTable_[labelBuffer_[index]];
	}
	std::fill_n(labelTable_.begin(), nextLabel, 0);
	for (std::size_t index = 0; index < pixelCount; ++index) {
		++labelTable_[labelBuffer_[index]];
	}
	for (std::uint32_t y = 0; y < destinationLayout.height; ++y) {
		auto *destinationRow = destination + static_cast<std::size_t>(y) * destinationLayout.linesize;
		for (std::uint32_t x = 0; x < destinationLayout.width; ++x) {
			const std::uint32_t label =
				labelBuffer_[static_cast<std::size_t>(y) * destinationLayout.width + x];
			destinationRow[x] = label != 0 && static_cast<double>(labelTable_[label]) > minimumArea ? 255
														: 0;
		}
	}
	return true;
}

} // namespace Operators
