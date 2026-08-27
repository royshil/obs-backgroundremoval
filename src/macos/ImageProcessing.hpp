// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#include <Accelerate/Accelerate.h>

#include "Memory/AlignedBuffer.hpp"

namespace ImageProcessing {

void thresholdBinaryMask(const float *source, std::uint8_t *destination, std::size_t pixelCount, float threshold);
void thresholdBinaryMask(std::uint8_t *mask, std::size_t pixelCount, std::uint8_t threshold);
void convertFloatMask(const vImage_Buffer &source, const vImage_Buffer &destination);

// Rosenfeld, A.; Pfaltz, J. L. "Sequential Operations in Digital Picture Processing."
// Journal of the ACM 13(4), 1966, pp. 471-494. https://doi.org/10.1145/321356.321357
template<std::size_t Width, std::size_t Height, typename LabelType> class ConnectedComponentFilter final {
	static_assert(Width > 0);
	static_assert(Height > 0);
	static_assert(std::is_unsigned_v<LabelType>);
	static_assert(Width <= std::numeric_limits<std::size_t>::max() / Height);
	static constexpr std::size_t pixelCount = Width * Height;
	static_assert(pixelCount <= std::numeric_limits<LabelType>::max());

public:
	ConnectedComponentFilter()
		: labels_(Memory::makeAlignedBytes(pixelCount * sizeof(LabelType))),
		  labelTable_(Memory::makeAlignedBytes((pixelCount + 1) * sizeof(LabelType)))
	{
	}

	ConnectedComponentFilter(const ConnectedComponentFilter &) = delete;
	ConnectedComponentFilter(ConnectedComponentFilter &&) = delete;
	auto operator=(const ConnectedComponentFilter &) -> ConnectedComponentFilter & = delete;
	auto operator=(ConnectedComponentFilter &&) -> ConnectedComponentFilter & = delete;

	void filter(const std::uint8_t *source, std::uint8_t *destination, double minimumArea)
	{
		auto *labels = reinterpret_cast<LabelType *>(labels_.get());
		auto *labelTable = reinterpret_cast<LabelType *>(labelTable_.get());
		std::fill_n(labels, pixelCount, LabelType{0});

		std::size_t nextLabel = 1;
		for (std::size_t y = 0; y < Height; ++y) {
			for (std::size_t x = 0; x < Width; ++x) {
				const std::size_t index = y * Width + x;
				if (source[index] == 0) {
					continue;
				}

				LabelType neighbors[4]{};
				std::size_t neighborCount = 0;
				if (x > 0 && labels[index - 1] != 0) {
					neighbors[neighborCount++] = labels[index - 1];
				}
				if (y > 0) {
					const std::size_t previousRow = index - Width;
					if (x > 0 && labels[previousRow - 1] != 0) {
						neighbors[neighborCount++] = labels[previousRow - 1];
					}
					if (labels[previousRow] != 0) {
						neighbors[neighborCount++] = labels[previousRow];
					}
					if (x + 1 < Width && labels[previousRow + 1] != 0) {
						neighbors[neighborCount++] = labels[previousRow + 1];
					}
				}

				if (neighborCount == 0) {
					const auto label = static_cast<LabelType>(nextLabel);
					labels[index] = label;
					labelTable[label] = label;
					++nextLabel;
				} else {
					const LabelType label = *std::min_element(neighbors, neighbors + neighborCount);
					labels[index] = label;
					for (std::size_t neighbor = 0; neighbor < neighborCount; ++neighbor) {
						mergeLabels(labelTable, label, neighbors[neighbor]);
					}
				}
			}
		}

		for (std::size_t label = 1; label < nextLabel; ++label) {
			const auto typedLabel = static_cast<LabelType>(label);
			labelTable[typedLabel] = findRoot(labelTable, typedLabel);
		}
		for (std::size_t index = 0; index < pixelCount; ++index) {
			if (labels[index] != 0) {
				labels[index] = labelTable[labels[index]];
			}
		}

		std::fill_n(labelTable, nextLabel, LabelType{0});
		for (std::size_t index = 0; index < pixelCount; ++index) {
			++labelTable[labels[index]];
		}

		std::fill_n(destination, pixelCount, std::uint8_t{0});
		for (std::size_t index = 0; index < pixelCount; ++index) {
			const LabelType label = labels[index];
			if (label != 0 && static_cast<double>(labelTable[label]) > minimumArea) {
				destination[index] = 255;
			}
		}
	}

private:
	static auto findRoot(LabelType *labelTable, LabelType label) -> LabelType
	{
		LabelType root = label;
		while (labelTable[root] != root) {
			root = labelTable[root];
		}
		while (labelTable[label] != label) {
			const LabelType parent = labelTable[label];
			labelTable[label] = root;
			label = parent;
		}
		return root;
	}

	static void mergeLabels(LabelType *labelTable, LabelType first, LabelType second)
	{
		const LabelType firstRoot = findRoot(labelTable, first);
		const LabelType secondRoot = findRoot(labelTable, second);
		if (firstRoot < secondRoot) {
			labelTable[secondRoot] = firstRoot;
		} else if (secondRoot < firstRoot) {
			labelTable[firstRoot] = secondRoot;
		}
	}

	Memory::AlignedBufferPtr labels_;
	Memory::AlignedBufferPtr labelTable_;
};

} // namespace ImageProcessing
