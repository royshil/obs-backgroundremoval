// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "PipelineFactory.hpp"

#include <BackgroundRemoval/MediaPipeLandscapeShader/MPLSPipeline.hpp>
#include <plugin-support.h>

namespace BackgroundRemoval {

auto createRenderingPipeline(std::int64_t pipelineId, obs_source_t *source, std::uint32_t sourceWidth,
			     std::uint32_t sourceHeight, std::int64_t outputDownscaleFactor)
	-> std::unique_ptr<IRenderingPipeline>
{
	if (pipelineId == kAutoPipelineId || pipelineId == kMPLSPipelineId) {
		obs_log(LOG_INFO, "Creating the MediaPipe (Landscape) shader pipeline (%ux%u, downscale factor %lld)",
			sourceWidth, sourceHeight, static_cast<long long>(outputDownscaleFactor));
		return std::make_unique<MPLSPipeline>(source, sourceWidth, sourceHeight, outputDownscaleFactor);
	}
	obs_log(LOG_ERROR, "Unknown rendering pipeline ID: %lld", static_cast<long long>(pipelineId));
	return nullptr;
}

} // namespace BackgroundRemoval
