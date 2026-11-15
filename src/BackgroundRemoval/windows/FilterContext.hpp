// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <obs-module.h>

namespace BackgroundRemoval {

/// Windows background-removal filter.
///
/// This filter currently provides only its properties. It passes video through unchanged.
class FilterContext final {
public:
	FilterContext(obs_data_t *settings, obs_source_t *source);
	~FilterContext() noexcept = default;

	FilterContext(const FilterContext &) = delete;
	FilterContext(FilterContext &&) = delete;
	auto operator=(const FilterContext &) -> FilterContext & = delete;
	auto operator=(FilterContext &&) -> FilterContext & = delete;

	static auto getName(void *) noexcept -> const char *;
	static auto create(obs_data_t *settings, obs_source_t *source) noexcept -> void *;
	static void destroy(void *data) noexcept;
	static void getDefaults(obs_data_t *settings) noexcept;
	static auto getProperties(void *data) noexcept -> obs_properties_t *;
	static void videoRender(void *data, gs_effect_t *effect) noexcept;

	static const obs_source_info kSourceInfo;

private:
	obs_source_t *const source_;

	static auto handleAboutButtonClicked(obs_properties_t *, obs_property_t *, void *) -> bool;
};

} // namespace BackgroundRemoval
