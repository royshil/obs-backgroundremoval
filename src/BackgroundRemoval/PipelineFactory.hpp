// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <memory>

#include <obs.h>

#include "IRenderingPipeline.hpp"

namespace BackgroundRemoval {

/// Creates the pipeline that a v2_PipelineId value selects; kAutoPipelineId selects the
/// recommended one. It processes frames of the target's size, sourceWidth x sourceHeight, and
/// outputs 1 / outputDownscaleFactor of it. The pipelines draw with effects compiled once per process, so
/// recreating one for a new size or downscale factor only allocates textures. Call it inside the OBS graphics
/// context.
/// @return The pipeline, or null when the ID is unknown.
/// @throws std::exception when the pipeline cannot be created.
auto createRenderingPipeline(std::int64_t pipelineId, obs_source_t *source, std::uint32_t sourceWidth,
			     std::uint32_t sourceHeight, std::int64_t outputDownscaleFactor)
	-> std::unique_ptr<IRenderingPipeline>;

} // namespace BackgroundRemoval
