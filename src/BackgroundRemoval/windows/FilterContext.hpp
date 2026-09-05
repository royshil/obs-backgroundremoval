// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "BackgroundRemoval/FilterProperty.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>

#include <obs-module.h>

namespace BackgroundRemoval {

class IRenderingPipeline;
class MainEffect;

class FilterContext final {
public:
	FilterContext(obs_data_t *settings, obs_source_t *source);
	~FilterContext() noexcept;

	FilterContext(const FilterContext &) = delete;
	FilterContext(FilterContext &&) = delete;
	auto operator=(const FilterContext &) -> FilterContext & = delete;
	auto operator=(FilterContext &&) -> FilterContext & = delete;

	static auto getSourceInfo() noexcept -> obs_source_info
	{
		obs_source_info sourceInfo{};
		sourceInfo.id = "background_removal";
		sourceInfo.type = OBS_SOURCE_TYPE_FILTER;
		sourceInfo.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW;
		sourceInfo.get_name = getName;
		sourceInfo.create = create;
		sourceInfo.destroy = destroy;
		sourceInfo.get_defaults = getDefaults;
		sourceInfo.get_properties = getProperties;
		sourceInfo.update = update;
		sourceInfo.video_tick = videoTick;
		sourceInfo.video_render = videoRender;
		return sourceInfo;
	}
	static auto getName(void *) noexcept -> const char *;
	static auto create(obs_data_t *settings, obs_source_t *source) noexcept -> void *;
	static void destroy(void *data) noexcept;
	static void getDefaults(obs_data_t *settings) noexcept;
	static auto getProperties(void *) noexcept -> obs_properties_t *;
	static void update(void *data, obs_data_t *settings) noexcept;
	static void videoTick(void *data, float seconds) noexcept;
	static void videoRender(void *data, gs_effect_t *effect) noexcept;

private:
	obs_source_t *const source_;
	std::optional<FilterProperty> pendingFilterProperty_;
	FilterProperty currentFilterProperty_{};
	std::unique_ptr<MainEffect> backgroundRemovalEffect_;
	mutable std::mutex renderingPipelineMutex_;
	std::shared_ptr<IRenderingPipeline> renderingPipeline_;
	std::atomic<long long> heldPipelineId_{};
	std::atomic<std::uint32_t> heldWidth_{};
	std::atomic<std::uint32_t> heldHeight_{};
};

} // namespace BackgroundRemoval
