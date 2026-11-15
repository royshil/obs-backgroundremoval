// SPDX-FileCopyrightText: 2025-2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TextureReader.hpp"

#include <cstring>
#include <stdexcept>

namespace ObsBridgeUtils {

TextureReader::TextureReader(std::uint32_t width, std::uint32_t height, gs_color_format format)
	: height_(height),
	  bufferLinesize_(width * getBytesPerPixel(format)),
	  buffer_(static_cast<std::size_t>(height) * bufferLinesize_),
	  stagesurface_(makeUniqueStagesurf(width, height, format))
{
	if (!stagesurface_) {
		throw std::runtime_error("Failed to create staging surface");
	}
}

TextureReader::~TextureReader() noexcept = default;

auto TextureReader::read(gs_texture_t *texture) noexcept -> bool
{
	if (!texture) {
		return false;
	}

	gs_stage_texture(stagesurface_.get(), texture);
	std::uint8_t *data = nullptr;
	std::uint32_t linesize = 0;
	if (!gs_stagesurface_map(stagesurface_.get(), &data, &linesize) || !data) {
		return false;
	}
	if (linesize < bufferLinesize_) {
		gs_stagesurface_unmap(stagesurface_.get());
		return false;
	}
	if (linesize == bufferLinesize_) {
		std::memcpy(buffer_.data(), data, static_cast<std::size_t>(height_) * bufferLinesize_);
	} else {
		for (std::uint32_t row = 0; row < height_; ++row) {
			std::memcpy(buffer_.data() + static_cast<std::size_t>(row) * bufferLinesize_,
				    data + static_cast<std::size_t>(row) * linesize, bufferLinesize_);
		}
	}
	gs_stagesurface_unmap(stagesurface_.get());
	return true;
}

auto TextureReader::getBuffer() const noexcept -> const std::vector<std::uint8_t> &
{
	return buffer_;
}

auto TextureReader::getBufferLinesize() const noexcept -> std::uint32_t
{
	return bufferLinesize_;
}

auto TextureReader::getBytesPerPixel(gs_color_format format) -> std::uint32_t
{
	switch (format) {
	case GS_A8:
	case GS_R8:
		return 1;
	case GS_R8G8:
	case GS_R16:
	case GS_R16F:
		return 2;
	case GS_RGBA:
	case GS_BGRA:
	case GS_BGRX:
	case GS_R10G10B10A2:
	case GS_R32F:
	case GS_RGBA_UNORM:
	case GS_BGRA_UNORM:
	case GS_BGRX_UNORM:
	case GS_RG16:
	case GS_RG16F:
		return 4;
	case GS_RGBA16:
	case GS_RGBA16F:
	case GS_RG32F:
		return 8;
	case GS_RGBA32F:
		return 16;
	default:
		throw std::runtime_error("Unsupported texture format");
	}
}

} // namespace ObsBridgeUtils
