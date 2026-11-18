// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <obs.h>

#include <string>

namespace BackgroundRemoval {

class FilterContext final {
public:
	FilterContext(obs_data_t *settings, obs_source_t *source);
	~FilterContext() noexcept = default;

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
		sourceInfo.video_render = videoRender;
		return sourceInfo;
	}
	static const char *getName(void *) noexcept;
	static void *create(obs_data_t *settings, obs_source_t *source) noexcept;
	static void destroy(void *data) noexcept;
	static void getDefaults(obs_data_t *settings) noexcept;
	static obs_properties_t *getProperties(void *data) noexcept;
	static void videoRender(void *data, gs_effect_t *effect) noexcept;

private:
	obs_source_t *const source_;

	static std::string getVersionNotice();
	static bool handleAboutButtonClicked(obs_properties_t *, obs_property_t *, void *);
};

} // namespace BackgroundRemoval
