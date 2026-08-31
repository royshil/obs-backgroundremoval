// SPDX-FileCopyrightText: 2025-2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "GsUnique.hpp"

#include <deque>
#include <mutex>
#include <utility>
#include <variant>

namespace ObsBridgeUtils {

namespace GsUnique {

namespace {

struct DrainTag final {};

using GsResource = std::variant<DrainTag, gs_stagesurf_t *, gs_texture_t *>;

void schedule(GsResource resource)
{
	static std::mutex mutex;
	static std::deque<GsResource> scheduledResources;

	if (std::holds_alternative<DrainTag>(resource)) {
		std::deque<GsResource> resources;
		{
			std::lock_guard lock(mutex);
			resources.swap(scheduledResources);
		}
		for (GsResource &scheduledResource : resources) {
			if (auto *surface = std::get_if<gs_stagesurf_t *>(&scheduledResource)) {
				gs_stagesurface_destroy(*surface);
			} else if (auto *texture = std::get_if<gs_texture_t *>(&scheduledResource)) {
				gs_texture_destroy(*texture);
			}
		}
	} else {
		std::lock_guard lock(mutex);
		scheduledResources.emplace_back(std::move(resource));
	}
}

} // namespace

void drain()
{
	schedule(DrainTag{});
}

void StagesurfaceDeleter::operator()(gs_stagesurf_t *surface) const noexcept
{
	if (surface) {
		schedule(GsResource{surface});
	}
}

void TextureDeleter::operator()(gs_texture_t *texture) const noexcept
{
	if (texture) {
		schedule(GsResource{texture});
	}
}

} // namespace GsUnique

auto makeUniqueStagesurf(std::uint32_t width, std::uint32_t height, gs_color_format format) -> UniqueStagesurfPtr
{
	return UniqueStagesurfPtr(gs_stagesurface_create(width, height, format));
}

auto makeUniqueTexture(std::uint32_t width, std::uint32_t height, gs_color_format format, std::uint32_t flags)
	-> UniqueTexturePtr
{
	return UniqueTexturePtr(gs_texture_create(width, height, format, 1, nullptr, flags));
}

} // namespace ObsBridgeUtils
