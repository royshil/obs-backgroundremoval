// SPDX-FileCopyrightText: 2025-2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <memory>

#include <obs.h>

namespace ObsBridgeUtils {

namespace GsUnique {

void drain();

struct StagesurfaceDeleter {
	void operator()(gs_stagesurf_t *surface) const noexcept;
};

struct TextureDeleter {
	void operator()(gs_texture_t *texture) const noexcept;
};

} // namespace GsUnique

using UniqueStagesurfPtr = std::unique_ptr<gs_stagesurf_t, GsUnique::StagesurfaceDeleter>;
using UniqueTexturePtr = std::unique_ptr<gs_texture_t, GsUnique::TextureDeleter>;

auto makeUniqueStagesurf(std::uint32_t width, std::uint32_t height, gs_color_format format) -> UniqueStagesurfPtr;
auto makeUniqueTexture(std::uint32_t width, std::uint32_t height, gs_color_format format, std::uint32_t flags)
	-> UniqueTexturePtr;

} // namespace ObsBridgeUtils
