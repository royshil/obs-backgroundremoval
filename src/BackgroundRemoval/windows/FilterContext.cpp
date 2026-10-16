// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "FilterContext.hpp"
#include "BackgroundRemoval/PipelineFactory.hpp"

#include <cstdint>
#include <cstring>
#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

#include <UI/AboutDialogIntegration.hpp>
#include <UpdateConfig/UpdateConfig.hpp>
#include "MediaPipeLandscapeDirectMLPipeline.hpp"

namespace BackgroundRemoval {

FilterContext::FilterContext(obs_data_t *settings, obs_source_t *source) : source_(source)
{
	if (!settings || !source_) {
		throw std::invalid_argument("Invalid Windows background-removal context arguments");
	}
	pendingFilterProperty_ = decodeFilterProperty(settings);
	currentFilterProperty_ = *pendingFilterProperty_;
}

FilterContext::~FilterContext() noexcept
{
	renderingPipeline_.reset();
}

auto FilterContext::getName(void *) noexcept -> const char *
{
	return obs_module_text("BackgroundRemoval");
}

auto FilterContext::create(obs_data_t *settings, obs_source_t *source) noexcept -> void *
{
	try {
		return new std::shared_ptr<FilterContext>(std::make_shared<FilterContext>(settings, source));
	} catch (const std::exception &exception) {
		blog(LOG_ERROR, OBS_LOG_HEADER "Failed to create Windows FilterContext: %s", exception.what());
	} catch (...) {
		blog(LOG_ERROR, OBS_LOG_HEADER "Failed to create Windows FilterContext");
	}
	return nullptr;
}

void FilterContext::destroy(void *data) noexcept
{
	delete static_cast<std::shared_ptr<FilterContext> *>(data);
}

void FilterContext::getDefaults(obs_data_t *settings) noexcept
{
	obs_data_set_default_int(settings, "v2_Model_PipelineId", MediaPipeLandscapeDirectMLPipeline::kPipelineId);
	obs_data_set_default_double(settings, "v2_Model_MatteSensitivity", 60.0);
	obs_data_set_default_bool(settings, "v2_RefineMask", true);
	obs_data_set_default_double(settings, "v2_RefineMask_Threshold", 0.5);
	obs_data_set_default_bool(settings, "v2_BlurBg", false);
	obs_data_set_default_int(settings, "v2_BlurBg_Factor", 4);
	obs_data_set_default_double(settings, "v2_BlurBg_FocusPoint", 0.1);
	obs_data_set_default_double(settings, "v2_BlurBg_FocusDepth", 0.0);
}

auto FilterContext::handleAutoSensitivityClick(obs_properties_t *, obs_property_t *, void *data) -> bool
{
	if (auto *contextPtr = static_cast<std::shared_ptr<FilterContext> *>(data); contextPtr && *contextPtr) {
		std::shared_ptr<MediaPipeLandscapeDirectMLPipeline> pipeline;
		{
			std::lock_guard lock((*contextPtr)->renderingPipelineMutex_);
			pipeline = std::dynamic_pointer_cast<MediaPipeLandscapeDirectMLPipeline>(
				(*contextPtr)->renderingPipeline_);
		}
		return pipeline && pipeline->autoSetSensitivity();
	} else {
		return false;
	}
}

auto FilterContext::getProperties(void *) noexcept -> obs_properties_t *
{
	obs_properties_t *const properties = obs_properties_create();
	const char *aboutButtonText;
	if (const std::optional<std::string> latestVersion = UpdateConfig::getLatestVersion();
	    latestVersion && *latestVersion != PLUGIN_VERSION_STR) {
		aboutButtonText = obs_module_text("About this plugin (New version available)");
	} else {
		aboutButtonText = obs_module_text("About this plugin");
	}

	obs_properties_add_button2(
		properties, "v2_AboutButton", aboutButtonText,
		[](obs_properties_t *, obs_property_t *, void *) {
			static_cast<void>(AboutDialogIntegration::show());
			return false;
		},
		nullptr);

	obs_properties_t *const modelProperties = obs_properties_create();
	obs_property_t *const model = obs_properties_add_list(modelProperties, "v2_Model_PipelineId", "Bg. Rm.",
							      OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(model, obs_module_text("MediaPipe (Landscape) - DirectML"),
				  MediaPipeLandscapeDirectMLPipeline::kPipelineId);
	obs_properties_add_float_slider(modelProperties, "v2_Model_MatteSensitivity", obs_module_text("Sensitivity"),
					0.0, 100.0, 0.01);
	obs_property_t *const autoSensitivity = obs_properties_add_button(modelProperties, "v2_Model_AutoSensitivity",
									  obs_module_text("AutoSetSensitivity"),
									  handleAutoSensitivityClick);
	obs_property_set_long_description(autoSensitivity, obs_module_text("AutoSetSensitivityDescription"));
	obs_properties_add_group(properties, "v2_Model", obs_module_text("Models"), OBS_GROUP_NORMAL, modelProperties);

	obs_properties_t *const blurProperties = obs_properties_create();
	obs_properties_add_int_slider(blurProperties, "v2_BlurBg_Factor", obs_module_text("Strength"), 0, 20, 1);
	obs_properties_add_float_slider(blurProperties, "v2_BlurBg_FocusPoint", obs_module_text("Focus point"), 0.0,
					1.0, 0.05);
	obs_properties_add_float_slider(blurProperties, "v2_BlurBg_FocusDepth", obs_module_text("Focus depth"), 0.0,
					0.3, 0.02);
	obs_properties_add_group(properties, "v2_BlurBg", obs_module_text("Blur Bg."), OBS_GROUP_CHECKABLE,
				 blurProperties);

	obs_properties_t *const thresholdProperties = obs_properties_create();
	obs_properties_add_float_slider(thresholdProperties, "v2_RefineMask_Threshold", obs_module_text("Threshold"),
					0.0, 1.0, 0.025);
	obs_properties_add_group(properties, "v2_RefineMask", obs_module_text("Refine Mask"), OBS_GROUP_CHECKABLE,
				 thresholdProperties);

	return properties;
}

void FilterContext::update(void *data, obs_data_t *settings) noexcept
{
	auto *contextPtr = static_cast<std::shared_ptr<FilterContext> *>(data);
	if (!contextPtr || !*contextPtr || !settings) {
		return;
	}
	const std::shared_ptr<FilterContext> context = *contextPtr;
	std::optional<FilterProperty> pendingFilterProperty{decodeFilterProperty(settings)};
	const FilterProperty &property = *pendingFilterProperty;
	blog(LOG_INFO,
	     OBS_LOG_HEADER
	     "Updating Windows settings: pipeline_id=%lld sensitivity=%.3f refine=%d threshold=%.3f blur=%d blur_factor=%lld focus_point=%.3f focus_depth=%.3f",
	     property.pipelineId, property.imageSimilarityThreshold, property.refineMaskEnabled,
	     property.refineMaskThreshold, property.blurBackgroundEnabled,
	     static_cast<long long>(property.blurBackgroundFactor), property.blurFocusPoint, property.blurFocusDepth);

	std::lock_guard lock(context->renderingPipelineMutex_);
	context->pendingFilterProperty_.swap(pendingFilterProperty);
}

void FilterContext::videoTick(void *data, float seconds) noexcept
{
	auto *contextPtr = static_cast<std::shared_ptr<FilterContext> *>(data);
	if (!contextPtr || !*contextPtr) {
		return;
	}
	const std::shared_ptr<FilterContext> context = *contextPtr;
	obs_source_t *const parent = obs_filter_get_parent(context->source_);
	obs_source_t *const target = obs_filter_get_target(context->source_);
	if (!parent || !target || !obs_source_active(parent)) {
		return;
	}
	const std::uint32_t width = obs_source_get_base_width(target);
	const std::uint32_t height = obs_source_get_base_height(target);
	if (width == 0 || height == 0) {
		return;
	}

	std::optional<FilterProperty> pendingFilterProperty;
	std::shared_ptr<IRenderingPipeline> heldPipeline;
	{
		std::lock_guard lock(context->renderingPipelineMutex_);
		pendingFilterProperty.swap(context->pendingFilterProperty_);
		heldPipeline = context->renderingPipeline_;
	}
	if (pendingFilterProperty) {
		context->currentFilterProperty_ = *pendingFilterProperty;
	}
	const long long heldPipelineId = context->heldPipelineId_.load(std::memory_order_relaxed);
	const std::uint32_t heldWidth = context->heldWidth_.load(std::memory_order_relaxed);
	const std::uint32_t heldHeight = context->heldHeight_.load(std::memory_order_relaxed);
	const bool pipelineChanged = heldPipelineId != context->currentFilterProperty_.pipelineId;
	const bool dimensionsChanged = heldWidth != width || heldHeight != height;
	const auto directMLPipeline = std::dynamic_pointer_cast<MediaPipeLandscapeDirectMLPipeline>(heldPipeline);
	const bool pipelineRequestedRecreation = directMLPipeline && directMLPipeline->needsRecreation();
	const bool requiresPipelineRecreation = pipelineChanged || dimensionsChanged || pipelineRequestedRecreation ||
						!heldPipeline;
	const bool requiresPipelineUpdate = pendingFilterProperty && !requiresPipelineRecreation;

	std::shared_ptr<IRenderingPipeline> pipeline;
	if (requiresPipelineRecreation) {
		const long long pipelineId = context->currentFilterProperty_.pipelineId;
		try {
			pipeline = createRenderingPipeline(context->currentFilterProperty_, context->source_, width,
							   height);
		} catch (const std::exception &exception) {
			blog(LOG_ERROR, OBS_LOG_HEADER "Failed to recreate DirectML rendering pipeline: %s",
			     exception.what());
		}
		if (pipeline) {
			context->heldPipelineId_.store(pipelineId, std::memory_order_relaxed);
			context->heldWidth_.store(width, std::memory_order_relaxed);
			context->heldHeight_.store(height, std::memory_order_relaxed);
		}
	}
	{
		std::lock_guard lock(context->renderingPipelineMutex_);
		if (requiresPipelineRecreation) {
			context->renderingPipeline_ = pipeline;
		} else {
			pipeline = context->renderingPipeline_;
		}
	}
	if (requiresPipelineUpdate && pipeline) {
		pipeline->update(*pendingFilterProperty);
	}
	if (pipeline) {
		if (auto directML = std::dynamic_pointer_cast<MediaPipeLandscapeDirectMLPipeline>(pipeline)) {
			obs_video_info videoInfo{};
			if (obs_get_video_info(&videoInfo) && videoInfo.fps_den && videoInfo.fps_num) {
				const std::uint64_t denominator = std::uint64_t(videoInfo.fps_den) * 30;
				directML->additionalInferenceFrames_.store(
					static_cast<std::uint32_t>((videoInfo.fps_num + denominator - 1) / denominator),
					std::memory_order_relaxed);
			}
		}
		pipeline->videoTick(seconds);
	}
}

void FilterContext::videoRender(void *data, gs_effect_t *effect) noexcept
{
	auto *contextPtr = static_cast<std::shared_ptr<FilterContext> *>(data);
	if (!contextPtr || !*contextPtr) {
		return;
	}
	const std::shared_ptr<FilterContext> context = *contextPtr;
	obs_source_t *const parent = obs_filter_get_parent(context->source_);
	if (!parent || !obs_source_active(parent) || !obs_source_showing(parent)) {
		return;
	}
	std::shared_ptr<IRenderingPipeline> pipeline;
	{
		std::lock_guard lock(context->renderingPipelineMutex_);
		pipeline = context->renderingPipeline_;
	}
	if (pipeline) {
		pipeline->videoRender(effect);
	}
}

} // namespace BackgroundRemoval
