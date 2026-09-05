// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "FilterProperty.hpp"

#include <cstdint>

#include <obs-module.h>

namespace BackgroundRemoval {

class IRenderingPipeline {
public:
	virtual ~IRenderingPipeline() noexcept = default;

	virtual auto pipelineId() const noexcept -> std::int32_t = 0;
	virtual void update(const FilterProperty &property) noexcept = 0;
	virtual void videoTick(float seconds) = 0;
	virtual void videoRender(gs_effect_t *effect) = 0;
	virtual auto getWidth() const noexcept -> std::uint32_t = 0;
	virtual auto getHeight() const noexcept -> std::uint32_t = 0;
};

} // namespace BackgroundRemoval
