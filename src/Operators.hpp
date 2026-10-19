// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <vector>

#include "Memory/AlignedBuffer.hpp"

namespace Operators {

struct ImageLayout {
	std::uint32_t width;
	std::uint32_t height;
	std::uint32_t linesize;
};

struct ImageSpec {
	std::uint32_t width;
	std::uint32_t height;
};

class PsnrF32 final {
public:
	auto calculate(const float *first, ImageLayout firstLayout, const float *second, ImageLayout secondLayout) const
		-> double;
};

class ResizeU8 final {
public:
	auto apply(const std::uint8_t *source, ImageLayout sourceLayout, std::uint8_t *destination,
		   ImageLayout destinationLayout) -> bool;

private:
	std::vector<std::uint8_t> temporaryBuffer_;
};

class SmoothU8 final {
public:
	explicit SmoothU8(ImageSpec spec);

	auto apply(const std::uint8_t *source, std::uint32_t sourceLinesize, std::uint8_t *destination,
		   std::uint32_t destinationLinesize) -> bool;

private:
	ImageSpec spec_;
	Memory::AlignedBufferPtr temporaryBuffer_;
};

class ThresholdU8 final {
public:
	explicit ThresholdU8(ImageSpec spec);

	auto apply(const std::uint8_t *source, std::uint32_t sourceLinesize, std::uint8_t *destination,
		   std::uint32_t destinationLinesize, std::uint8_t threshold) const noexcept -> bool;

private:
	ImageSpec spec_;
};

class ThresholdF32 final {
public:
	explicit ThresholdF32(ImageSpec spec);

	auto apply(const float *source, std::uint32_t sourceLinesize, std::uint8_t *destination,
		   std::uint32_t destinationLinesize, float threshold) const noexcept -> bool;

private:
	ImageSpec spec_;
};

class MorphologyU8 final {
public:
	explicit MorphologyU8(ImageSpec spec);

	auto begin(const std::uint8_t *source, std::uint32_t sourceLinesize) -> bool;
	auto erode() -> bool;
	auto dilate() -> bool;
	auto end(std::uint8_t *destination, std::uint32_t destinationLinesize) -> bool;

private:
	ImageSpec spec_;
	std::uint32_t internalLinesize_;
	Memory::AlignedBufferPtr firstBuffer_;
	Memory::AlignedBufferPtr secondBuffer_;
	Memory::AlignedBufferPtr temporaryBuffer_;
};

class RemoveSmallComponentsU8 final {
public:
	auto apply(const std::uint8_t *source, ImageLayout sourceLayout, std::uint8_t *destination,
		   ImageLayout destinationLayout, double minimumArea) -> bool;

private:
	std::vector<std::uint32_t> labelBuffer_;
	std::vector<std::uint32_t> labelTable_;
};

} // namespace Operators
