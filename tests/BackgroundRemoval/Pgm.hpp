// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Binary PGM (P5, 8 bits) files, as tests/data/GenerateMPLSReference.py writes the references.

#pragma once

#include <cstdint>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace TestPgm {

struct Gray {
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	std::vector<std::uint8_t> values;
};

inline auto read(const std::string &path) -> Gray
{
	std::ifstream file(path, std::ios::binary);
	std::string magic;
	unsigned width = 0;
	unsigned height = 0;
	unsigned maxValue = 0;
	if (!(file >> magic >> width >> height >> maxValue) || magic != "P5" || maxValue != 255) {
		throw std::runtime_error("Not an 8-bit binary PGM: " + path);
	}
	file.get();
	Gray gray{width, height, std::vector<std::uint8_t>(static_cast<std::size_t>(width) * height)};
	if (!file.read(reinterpret_cast<char *>(gray.values.data()),
		       static_cast<std::streamsize>(gray.values.size()))) {
		throw std::runtime_error("Truncated PGM: " + path);
	}
	return gray;
}

/// Samples the values bilinearly at the texel centers of a width x height frame, as a linear
/// sampler with clamped addressing does, and returns where the result is at least 0.5.
inline auto upscaledForeground(const std::vector<float> &values, std::uint32_t valuesWidth, std::uint32_t valuesHeight,
			       std::uint32_t width, std::uint32_t height) -> std::vector<bool>
{
	std::vector<bool> foreground(static_cast<std::size_t>(width) * height);
	auto at = [&](int x, int y) {
		x = x < 0 ? 0 : (x >= static_cast<int>(valuesWidth) ? static_cast<int>(valuesWidth) - 1 : x);
		y = y < 0 ? 0 : (y >= static_cast<int>(valuesHeight) ? static_cast<int>(valuesHeight) - 1 : y);
		return values[static_cast<std::size_t>(y) * valuesWidth + static_cast<std::size_t>(x)];
	};
	for (std::uint32_t y = 0; y < height; ++y) {
		const double sy = (y + 0.5) * valuesHeight / height - 0.5;
		const int y0 = static_cast<int>(sy < 0 ? sy - 1 : sy);
		const double fy = sy - y0;
		for (std::uint32_t x = 0; x < width; ++x) {
			const double sx = (x + 0.5) * valuesWidth / width - 0.5;
			const int x0 = static_cast<int>(sx < 0 ? sx - 1 : sx);
			const double fx = sx - x0;
			const double top = at(x0, y0) * (1 - fx) + at(x0 + 1, y0) * fx;
			const double bottom = at(x0, y0 + 1) * (1 - fx) + at(x0 + 1, y0 + 1) * fx;
			foreground[static_cast<std::size_t>(y) * width + x] = top * (1 - fy) + bottom * fy >= 0.5;
		}
	}
	return foreground;
}

inline auto toUnit(const std::vector<std::uint8_t> &values) -> std::vector<float>
{
	std::vector<float> unit(values.size());
	for (std::size_t i = 0; i < values.size(); ++i) {
		unit[i] = static_cast<float>(values[i]) / 255.0F;
	}
	return unit;
}

} // namespace TestPgm
