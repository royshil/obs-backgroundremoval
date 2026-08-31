// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

#include <obs-module.h>

namespace BackgroundRemoval {

class IRenderingPipeline {
public:
	virtual ~IRenderingPipeline() noexcept = default;

	virtual void update(obs_data_t *settings) noexcept = 0;
	virtual void videoTick(float seconds) = 0;
	virtual void videoRender(gs_effect_t *effect) = 0;
	virtual auto getWidth() const noexcept -> std::uint32_t = 0;
	virtual auto getHeight() const noexcept -> std::uint32_t = 0;
};

} // namespace BackgroundRemoval
