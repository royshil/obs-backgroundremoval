// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <memory>

struct obs_source;
using obs_source_t = struct obs_source;

namespace BackgroundRemoval {

struct FilterProperty;
class IRenderingPipeline;
class MainEffect;

auto createRenderingPipeline(const FilterProperty &property, obs_source_t *source, MainEffect &backgroundRemovalEffect,
			     std::uint32_t width, std::uint32_t height) -> std::shared_ptr<IRenderingPipeline>;

} // namespace BackgroundRemoval
