// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <obs.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "PipelineFactory.hpp"

namespace BackgroundRemoval {

class IRenderingPipeline;

class FilterContext final {
public:
	FilterContext(obs_data_t *settings, obs_source_t *source);
	~FilterContext() noexcept;

	// No copying allowed
	FilterContext(const FilterContext &) = delete;
	FilterContext &operator=(const FilterContext &) = delete;

	// No moving allowed
	FilterContext(FilterContext &&) = delete;
	FilterContext &operator=(FilterContext &&) = delete;

	static auto getSourceInfo() noexcept -> obs_source_info
	{
		obs_source_info sourceInfo{};
		sourceInfo.id = "background_removal";
		sourceInfo.type = OBS_SOURCE_TYPE_FILTER;
		sourceInfo.output_flags = OBS_SOURCE_VIDEO;
		sourceInfo.get_name = getName;
		sourceInfo.create = create;
		sourceInfo.destroy = destroy;
		sourceInfo.get_defaults = getDefaults;
		sourceInfo.get_properties = getProperties;
		sourceInfo.update = update;
		sourceInfo.get_width = getWidth;
		sourceInfo.get_height = getHeight;
		sourceInfo.video_tick = videoTick;
		sourceInfo.video_render = videoRender;
		return sourceInfo;
	}
	static const char *getName(void *) noexcept;
	static void *create(obs_data_t *settings, obs_source_t *source) noexcept;
	static void destroy(void *data) noexcept;
	static void getDefaults(obs_data_t *settings) noexcept;
	static obs_properties_t *getProperties(void *data) noexcept;
	static void update(void *data, obs_data_t *settings) noexcept;
	static std::uint32_t getWidth(void *data) noexcept;
	static std::uint32_t getHeight(void *data) noexcept;
	static void videoTick(void *data, float seconds) noexcept;
	static void videoRender(void *data, gs_effect_t *effect) noexcept;

private:
	/// What a pipeline is created for; any change recreates it.
	struct PipelineKey {
		std::int64_t pipelineId = 0;
		std::int64_t outputDownscaleFactor = 1;
		std::uint32_t sourceWidth = 0;
		std::uint32_t sourceHeight = 0;
		auto operator==(const PipelineKey &) const -> bool = default;
	};

	void readSettings(obs_data_t *settings) noexcept;
	void applySettings(IRenderingPipeline &pipeline) const noexcept;
	void tick() noexcept;
	void resetPipeline() noexcept;
	void render() noexcept;

	obs_source_t *const source_;

	// Written by update() on the UI thread, read on the graphics thread and by getWidth() and
	// getHeight() on any thread.
	std::atomic<std::int64_t> pipelineId_{0};
	std::atomic<std::int64_t> outputDownscaleFactor_{1};
	// Written by video_tick on the graphics thread, read by getWidth() and getHeight() on any thread:
	// the output size of pipeline_, or 0 without one.
	std::atomic<std::uint32_t> outputWidth_{0};
	std::atomic<std::uint32_t> outputHeight_{0};
	std::atomic<bool> blurBgEnabled_{false};
	std::atomic<double> blurBgStrength_{4.0};
	std::atomic<double> blurBgFocusRatio_{-100.0};

	// Graphics thread only: video_tick and video_render both run there.
	std::unique_ptr<IRenderingPipeline> pipeline_;
	/// What pipeline_ was created for, or failed to be created for.
	std::optional<PipelineKey> pipelineKey_;

	static std::string getVersionNotice();
	static bool handleAboutButtonClicked(obs_properties_t *, obs_property_t *, void *);
};

} // namespace BackgroundRemoval
