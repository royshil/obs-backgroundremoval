// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <memory>

struct obs_source;
using obs_source_t = struct obs_source;

namespace Effects {
class BackgroundRemovalEffect;
}

namespace BackgroundRemoval {

struct FilterProperty;
class IRenderingPipeline;

auto createRenderingPipeline(const FilterProperty &property, obs_source_t *source,
			     Effects::BackgroundRemovalEffect &backgroundRemovalEffect, std::uint32_t width,
			     std::uint32_t height) -> std::shared_ptr<IRenderingPipeline>;

} // namespace BackgroundRemoval
