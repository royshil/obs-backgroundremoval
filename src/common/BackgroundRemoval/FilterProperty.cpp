// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "FilterProperty.hpp"

namespace BackgroundRemoval {

auto decodeFilterProperty(obs_data_t *settings) noexcept -> FilterProperty
{
	if (!settings) {
		return {};
	}
	return {
		.pipelineId = obs_data_get_int(settings, "v2_Model_PipelineId"),
		.imageSimilarityThreshold = obs_data_get_double(settings, "v2_Model_MatteSensitivity"),
		.refineMaskEnabled = obs_data_get_bool(settings, "v2_RefineMask"),
		.refineMaskThreshold = obs_data_get_double(settings, "v2_RefineMask_Threshold"),
		.refineMaskContour = obs_data_get_double(settings, "v2_RefineMask_Contour"),
		.refineMaskExpansion = obs_data_get_int(settings, "v2_RefineMask_Expansion"),
		.blurBackgroundEnabled = obs_data_get_bool(settings, "v2_BlurBg"),
		.blurBackgroundFactor = obs_data_get_int(settings, "v2_BlurBg_Factor"),
		.blurFocusPoint = obs_data_get_double(settings, "v2_BlurBg_FocusPoint"),
		.blurFocusDepth = obs_data_get_double(settings, "v2_BlurBg_FocusDepth"),
	};
}

void encodeFilterProperty(obs_data_t *settings, const FilterProperty &property) noexcept
{
	if (!settings) {
		return;
	}
	obs_data_set_int(settings, "v2_Model_PipelineId", property.pipelineId);
	obs_data_set_double(settings, "v2_Model_MatteSensitivity", property.imageSimilarityThreshold);
	obs_data_set_bool(settings, "v2_RefineMask", property.refineMaskEnabled);
	obs_data_set_double(settings, "v2_RefineMask_Threshold", property.refineMaskThreshold);
	obs_data_set_double(settings, "v2_RefineMask_Contour", property.refineMaskContour);
	obs_data_set_int(settings, "v2_RefineMask_Expansion", property.refineMaskExpansion);
	obs_data_set_bool(settings, "v2_BlurBg", property.blurBackgroundEnabled);
	obs_data_set_int(settings, "v2_BlurBg_Factor", property.blurBackgroundFactor);
	obs_data_set_double(settings, "v2_BlurBg_FocusPoint", property.blurFocusPoint);
	obs_data_set_double(settings, "v2_BlurBg_FocusDepth", property.blurFocusDepth);
}

} // namespace BackgroundRemoval
