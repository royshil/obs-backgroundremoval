// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <memory>

#include <obs-module.h>

namespace BackgroundRemoval {

inline constexpr std::int32_t kMPLD10PipelineId = 62828;

class MotionMeasurement;

class IRenderingPipeline {
public:
	virtual ~IRenderingPipeline() noexcept = default;

	// MARK: - Pipeline properties
	virtual auto pipelineId() const noexcept -> std::int32_t = 0;
	virtual auto width() const noexcept -> std::uint32_t = 0;
	virtual auto height() const noexcept -> std::uint32_t = 0;
	virtual auto needsRecreation() const noexcept -> bool = 0;

	// MARK: - OBS Lifecycle Handlers
	virtual void videoTick(float seconds) noexcept = 0;
	virtual void videoRender(gs_effect_t *effect) noexcept = 0;

	// MARK: - General parameters
	virtual void setMotionSuppression(double value) noexcept = 0;
	virtual void setMotionMeasurement(std::shared_ptr<MotionMeasurement> measurement) noexcept = 0;
	virtual void setMaskAdjustment(double value) noexcept = 0;

	// MARK: - Blur background parameters
	virtual void setBlurBgEnabled(bool value) noexcept = 0;
	virtual void setBlurBgFactor(std::int64_t value) noexcept = 0;
	virtual void setBlurBgFocusPoint(double value) noexcept = 0;
	virtual void setBlurBgFocusDepth(double value) noexcept = 0;
};

} // namespace BackgroundRemoval
