// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "BackgroundRemoval/PipelineFactory.hpp"
#include "MediaPipeLandscapeDirectMLPipeline.hpp"

#include "BackgroundRemoval/MainEffect.hpp"
#include <obs-module.h>

namespace BackgroundRemoval {

auto createRenderingPipeline(const FilterProperty &property, obs_source_t *source, MainEffect &backgroundRemovalEffect,
			     std::uint32_t width, std::uint32_t height) -> std::shared_ptr<IRenderingPipeline>
{
	if (property.pipelineId == MediaPipeLandscapeDirectMLPipeline::kPipelineId) {
		blog(LOG_INFO, OBS_LOG_HEADER "Recreating DirectML rendering pipeline (%ux%u)", width, height);
		return std::make_shared<MediaPipeLandscapeDirectMLPipeline>(property, source, backgroundRemovalEffect,
									    width, height);
	}
	blog(LOG_ERROR, OBS_LOG_HEADER "Unknown rendering pipeline ID: %lld", property.pipelineId);
	return nullptr;
}

} // namespace BackgroundRemoval
