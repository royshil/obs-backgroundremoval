// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "BackgroundRemoval/PipelineFactory.hpp"
#include "MediaPipeLandscapeDirectML/MPLDPipeline.hpp"

#include <obs-module.h>

namespace BackgroundRemoval {

auto createRenderingPipeline(const FilterProperty &property, obs_source_t *source, std::uint32_t width,
			     std::uint32_t height) -> std::shared_ptr<IRenderingPipeline>
{
	if (property.pipelineId == MPLDPipeline::kPipelineId) {
		blog(LOG_INFO, OBS_LOG_HEADER "Recreating DirectML rendering pipeline (%ux%u)", width, height);
		obs_enter_graphics();
		try {
			auto pipeline =
				std::make_shared<MPLDPipeline>(property, source, width, height);
			obs_leave_graphics();
			return pipeline;
		} catch (...) {
			obs_leave_graphics();
			throw;
		}
	}
	blog(LOG_ERROR, OBS_LOG_HEADER "Unknown rendering pipeline ID: %lld", property.pipelineId);
	return nullptr;
}

auto createRenderingPipeline(const FilterProperty &property, obs_source_t *source, MainEffect &, std::uint32_t width,
			     std::uint32_t height) -> std::shared_ptr<IRenderingPipeline>
{
	return createRenderingPipeline(property, source, width, height);
}
} // namespace BackgroundRemoval
