// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "IRenderingPipeline.hpp"
#include "AutoConfigurator.hpp"
#include "MotionMeasurement.hpp"
#include <QPointer>

#include <atomic>
#include <cstdint>
#include <memory>

#include <obs-module.h>

namespace BackgroundRemoval {

class IRenderingPipeline;

class FilterContext final {
public:
	FilterContext(obs_data_t *settings, obs_source_t *source);
	~FilterContext() noexcept;

	FilterContext(const FilterContext &) = delete;
	FilterContext(FilterContext &&) = delete;
	auto operator=(const FilterContext &) -> FilterContext & = delete;
	auto operator=(FilterContext &&) -> FilterContext & = delete;

	static auto getName(void *) noexcept -> const char *;
	static auto create(obs_data_t *settings, obs_source_t *source) noexcept -> void *;
	static void destroy(void *data) noexcept;
	static void getDefaults(obs_data_t *settings) noexcept;
	static auto getProperties(void *) noexcept -> obs_properties_t *;
	static void update(void *data, obs_data_t *settings) noexcept;
	static void videoTick(void *data, float seconds) noexcept;
	static void videoRender(void *data, gs_effect_t *effect) noexcept;

	static const obs_source_info kSourceInfo;

private:
	friend class AutoConfigurator;
	friend struct FilterContextTestAccess;
	obs_source_t *const source_;
	QPointer<AutoConfigurator> autoConfigurator_; // UI thread only; Qt owns the dialog.
	MotionMeasurement motionMeasurement_;
	std::atomic<std::shared_ptr<IRenderingPipeline>> renderingPipeline_;

	std::atomic<std::int64_t> pipelineId_{62828};
	std::atomic<double> motionSuppression_{0.0};
	std::atomic<double> maskAdjustment_{0.0};
	std::atomic<bool> blurBgEnabled_{false};
	std::atomic<std::int64_t> blurBgFactor_{4};
	std::atomic<double> blurBgFocusPoint_{0.1};
	std::atomic<double> blurBgFocusDepth_{0.0};
	// Requested selection (Auto or explicit ID), accessed only by videoTick().
	std::int64_t activePipelineSelection_ = -1;

	void readSettings(obs_data_t *settings) noexcept;
	void applyRenderingSettings(IRenderingPipeline &pipeline) const noexcept;

	static auto handleAboutButtonClicked(obs_properties_t *, obs_property_t *, void *) -> bool;
	static auto handleAutoSensitivityClick(obs_properties_t *, obs_property_t *, void *) -> bool;
};

} // namespace BackgroundRemoval
