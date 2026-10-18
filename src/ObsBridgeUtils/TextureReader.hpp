// SPDX-FileCopyrightText: 2025-2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <vector>

#include "GsUnique.hpp"

namespace ObsBridgeUtils {

class TextureReader final {
public:
	TextureReader(std::uint32_t width, std::uint32_t height, gs_color_format format);
	~TextureReader() noexcept;

	TextureReader(const TextureReader &) = delete;
	auto operator=(const TextureReader &) -> TextureReader & = delete;
	TextureReader(TextureReader &&) = delete;
	auto operator=(TextureReader &&) -> TextureReader & = delete;

	auto read(gs_texture_t *texture) noexcept -> bool;
	auto getBuffer() const noexcept -> const std::vector<std::uint8_t> &;
	auto getBufferLinesize() const noexcept -> std::uint32_t;

private:
	static auto getBytesPerPixel(gs_color_format format) -> std::uint32_t;

	const std::uint32_t height_;
	const std::uint32_t bufferLinesize_;
	std::vector<std::uint8_t> buffer_;
	UniqueStagesurfPtr stagesurface_;
};

} // namespace ObsBridgeUtils
