// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "PipelineFactory.hpp"
#include "MPLD10Pipeline.hpp"

#include <obs-module.h>

namespace BackgroundRemoval {

auto createRenderingPipeline(std::int64_t pipelineId, obs_source_t *source, std::uint32_t width, std::uint32_t height)
	-> std::shared_ptr<IRenderingPipeline>
{
	if (pipelineId == 0 || pipelineId == kMPLD10PipelineId) {
		blog(LOG_INFO, OBS_LOG_HEADER "Recreating DirectML rendering pipeline (%ux%u)", width, height);
		obs_enter_graphics();
		try {
			auto pipeline = std::make_shared<MPLD10Pipeline>(source, width, height);
			obs_leave_graphics();
			return pipeline;
		} catch (...) {
			obs_leave_graphics();
			throw;
		}
	}
	blog(LOG_ERROR, OBS_LOG_HEADER "Unknown rendering pipeline ID: %lld", pipelineId);
	return nullptr;
}

} // namespace BackgroundRemoval
