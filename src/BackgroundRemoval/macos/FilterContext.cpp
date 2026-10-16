// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "FilterContext.hpp"
#include "MediaPipeLandscapeCoreMLPipeline.hpp"
#include "BackgroundRemoval/IRenderingPipeline.hpp"
#include "BackgroundRemoval/PipelineFactory.hpp"

#include <cstdint>
#include <cstring>
#include <exception>
#include <optional>
#include <stdexcept>
#include <string>

#include <ObsBridgeUtils/GsUnique.hpp>
#include "BackgroundRemoval/MainEffect.hpp"
#include <UI/AboutDialogIntegration.hpp>
#include <UpdateConfig/UpdateConfig.hpp>
#include <plugin-support.h>

namespace BackgroundRemoval {

auto FilterContext::osLogger() noexcept -> os_log_t
{
	static os_log_t logger = os_log_create("com.royshil.obs-backgroundremoval", "FilterContext");
	return logger;
}

FilterContext::FilterContext(obs_data_t *settings, obs_source_t *source) : source_(source)
{
	if (!settings) {
		os_log_with_type(osLogger(), OS_LOG_TYPE_ERROR, "Background removal settings are null");
		throw std::invalid_argument("Background removal settings are null");
	} else if (!source_) {
		os_log_with_type(osLogger(), OS_LOG_TYPE_ERROR, "Background removal source is null");
		throw std::invalid_argument("Background removal source is null");
	}

	pendingFilterProperty_ = decodeFilterProperty(settings);
	currentFilterProperty_ = *pendingFilterProperty_;

	ObsBridgeUtils::GsUnique::GraphicsGuard graphicsGuard;
	backgroundRemovalEffect_ = std::make_unique<MainEffect>();
}

FilterContext::~FilterContext() noexcept
{
	renderingPipeline_.reset();
	ObsBridgeUtils::GsUnique::GraphicsGuard graphicsGuard;
	backgroundRemovalEffect_.reset();
	ObsBridgeUtils::GsUnique::drain();
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
		blog(LOG_ERROR, OBS_LOG_HEADER "Failed to create FilterContext: %s", exception.what());
		os_log_with_type(osLogger(), OS_LOG_TYPE_ERROR, "Failed to create FilterContext: %{public}s",
				 exception.what());
	} catch (...) {
		blog(LOG_ERROR, OBS_LOG_HEADER "Failed to create FilterContext");
		os_log_with_type(osLogger(), OS_LOG_TYPE_ERROR, "Failed to create FilterContext");
	}

	return nullptr;
}

void FilterContext::destroy(void *data) noexcept
{
	delete static_cast<std::shared_ptr<FilterContext> *>(data);
}

void FilterContext::getDefaults(obs_data_t *settings) noexcept
{
	obs_data_set_default_int(settings, "v2_Model_PipelineId", MediaPipeLandscapeCoreMLPipeline::kPipelineId);
	obs_data_set_default_double(settings, "v2_Model_MatteSensitivity", 35.0);
	obs_data_set_default_bool(settings, "v2_RefineMask", true);
	obs_data_set_default_double(settings, "v2_RefineMask_Threshold", 0.5);
	obs_data_set_default_double(settings, "v2_RefineMask_Contour", 0.05);
	obs_data_set_default_int(settings, "v2_RefineMask_Expansion", 0);
	obs_data_set_default_bool(settings, "v2_BlurBg", false);
	obs_data_set_default_int(settings, "v2_BlurBg_Factor", 4);
	obs_data_set_default_double(settings, "v2_BlurBg_FocusPoint", 0.1);
	obs_data_set_default_double(settings, "v2_BlurBg_FocusDepth", 0.0);
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

	obs_properties_t *const blurBgProps = obs_properties_create();
	obs_properties_add_int_slider(blurBgProps, "v2_BlurBg_Factor", obs_module_text("Strength"), 0, 20, 1);
	obs_properties_add_float_slider(blurBgProps, "v2_BlurBg_FocusPoint", obs_module_text("Focus point"), 0.0, 1.0,
					0.05);
	obs_properties_add_float_slider(blurBgProps, "v2_BlurBg_FocusDepth", obs_module_text("Focus depth"), 0.0, 0.3,
					0.02);
	obs_properties_add_group(properties, "v2_BlurBg", obs_module_text("Blur Bg."), OBS_GROUP_CHECKABLE,
				 blurBgProps);

	obs_properties_t *const thresholdProps = obs_properties_create();
	obs_properties_add_float_slider(thresholdProps, "v2_RefineMask_Threshold", obs_module_text("Threshold"), 0.0,
					1.0, 0.025);
	obs_properties_add_float_slider(thresholdProps, "v2_RefineMask_Contour", obs_module_text("Contour"), 0.0, 1.0,
					0.025);
	obs_properties_add_int_slider(thresholdProps, "v2_RefineMask_Expansion", obs_module_text("Expansion"), -30, 30,
				      1);
	obs_properties_add_group(properties, "v2_RefineMask", obs_module_text("Refine Mask"), OBS_GROUP_CHECKABLE,
				 thresholdProps);

	obs_properties_t *const modelProps = obs_properties_create();
	obs_property_t *const model = obs_properties_add_list(
		modelProps, "v2_Model_PipelineId", obs_module_text("Model"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(model, obs_module_text("MediaPipe Selfie (Landscape) - CoreML"),
				  MediaPipeLandscapeCoreMLPipeline::kPipelineId);
	obs_properties_add_float_slider(modelProps, "v2_Model_MatteSensitivity", obs_module_text("Sensitivity"), 0.0,
					100.0, 1.0);
	obs_properties_add_group(properties, "v2_Model", obs_module_text("Models"), OBS_GROUP_NORMAL, modelProps);

	return properties;
}

void FilterContext::update(void *data, obs_data_t *settings) noexcept
{
	auto *contextPtr = static_cast<std::shared_ptr<FilterContext> *>(data);
	if (!contextPtr || !*contextPtr) {
		return;
	}
	std::shared_ptr<FilterContext> context = *contextPtr;
	if (!settings) {
		blog(LOG_ERROR, OBS_LOG_HEADER "Background removal settings are null");
		os_log_with_type(osLogger(), OS_LOG_TYPE_ERROR, "Background removal settings are null");
		return;
	}
	std::optional<FilterProperty> pendingFilterProperty{decodeFilterProperty(settings)};
	const FilterProperty &property = *pendingFilterProperty;
	blog(LOG_INFO,
	     OBS_LOG_HEADER
	     "Updating settings: pipeline_id=%lld sensitivity=%.3f refine=%d threshold=%.3f contour=%.3f expansion=%lld blur=%d blur_factor=%lld focus_point=%.3f focus_depth=%.3f",
	     property.pipelineId, property.imageSimilarityThreshold, property.refineMaskEnabled,
	     property.refineMaskThreshold, property.refineMaskContour,
	     static_cast<long long>(property.refineMaskExpansion), property.blurBackgroundEnabled,
	     static_cast<long long>(property.blurBackgroundFactor), property.blurFocusPoint, property.blurFocusDepth);
	os_log_with_type(
		osLogger(), OS_LOG_TYPE_INFO,
		"Updating settings: pipeline_id=%{public}lld sensitivity=%{public}.3f refine=%{public}d threshold=%{public}.3f contour=%{public}.3f expansion=%{public}lld blur=%{public}d blur_factor=%{public}lld focus_point=%{public}.3f focus_depth=%{public}.3f",
		property.pipelineId, property.imageSimilarityThreshold, property.refineMaskEnabled,
		property.refineMaskThreshold, property.refineMaskContour,
		static_cast<long long>(property.refineMaskExpansion), property.blurBackgroundEnabled,
		static_cast<long long>(property.blurBackgroundFactor), property.blurFocusPoint,
		property.blurFocusDepth);
	std::lock_guard lock(context->renderingPipelineMutex_);
	context->pendingFilterProperty_.swap(pendingFilterProperty);
}

void FilterContext::videoTick(void *data, float seconds) noexcept
{
	auto *contextPtr = static_cast<std::shared_ptr<FilterContext> *>(data);
	if (!contextPtr || !*contextPtr) {
		return;
	}
	std::shared_ptr<FilterContext> context = *contextPtr;

	obs_source_t *const parent = obs_filter_get_parent(context->source_);
	if (!parent || !obs_source_active(parent)) {
		return;
	}

	obs_source_t *const target = obs_filter_get_target(context->source_);
	if (!target) {
		return;
	}

	const std::uint32_t targetWidth = obs_source_get_base_width(target);
	const std::uint32_t targetHeight = obs_source_get_base_height(target);
	if (targetWidth == 0 || targetHeight == 0) {
		return;
	}

	std::optional<FilterProperty> pendingFilterProperty;
	{
		std::lock_guard lock(context->renderingPipelineMutex_);
		pendingFilterProperty.swap(context->pendingFilterProperty_);
	}
	if (pendingFilterProperty) {
		context->currentFilterProperty_ = *pendingFilterProperty;
	}
	const long long heldPipelineId = context->heldPipelineId_.load(std::memory_order_relaxed);
	const std::uint32_t heldWidth = context->heldWidth_.load(std::memory_order_relaxed);
	const std::uint32_t heldHeight = context->heldHeight_.load(std::memory_order_relaxed);
	const bool pipelineChanged = heldPipelineId != context->currentFilterProperty_.pipelineId;
	const bool dimensionsChanged = heldWidth != targetWidth || heldHeight != targetHeight;
	const bool requiresPipelineRecreation = pipelineChanged || dimensionsChanged;
	const bool requiresPipelineUpdate = pendingFilterProperty && !requiresPipelineRecreation;

	std::shared_ptr<IRenderingPipeline> renderingPipeline;
	if (requiresPipelineRecreation) {
		const long long pipelineId = context->currentFilterProperty_.pipelineId;
		try {
			renderingPipeline = createRenderingPipeline(context->currentFilterProperty_, context->source_,
								    *context->backgroundRemovalEffect_, targetWidth,
								    targetHeight);
		} catch (const std::exception &exception) {
			blog(LOG_ERROR, OBS_LOG_HEADER "Failed to create rendering context: %s", exception.what());
			os_log_with_type(osLogger(), OS_LOG_TYPE_ERROR,
					 "Failed to create rendering context: %{public}s", exception.what());
		}
		if (renderingPipeline) {
			context->heldPipelineId_.store(pipelineId, std::memory_order_relaxed);
			context->heldWidth_.store(targetWidth, std::memory_order_relaxed);
			context->heldHeight_.store(targetHeight, std::memory_order_relaxed);
		}
	}
	{
		std::lock_guard lock(context->renderingPipelineMutex_);
		if (requiresPipelineRecreation) {
			context->renderingPipeline_ = renderingPipeline;
		} else {
			renderingPipeline = context->renderingPipeline_;
		}
	}
	if (requiresPipelineUpdate && renderingPipeline) {
		renderingPipeline->update(*pendingFilterProperty);
	}
	if (renderingPipeline) {
		renderingPipeline->videoTick(seconds);
	}
}

void FilterContext::videoRender(void *data, gs_effect_t *effect) noexcept
{
	using namespace ObsBridgeUtils;

	GsUnique::drain();

	auto *contextPtr = static_cast<std::shared_ptr<FilterContext> *>(data);
	if (!contextPtr || !*contextPtr) {
		return;
	}
	std::shared_ptr<FilterContext> context = *contextPtr;

	obs_source_t *const parent = obs_filter_get_parent(context->source_);
	if (!parent || !obs_source_active(parent) || !obs_source_showing(parent)) {
		// Draw nothing to prevent unexpected background disclosure.
		return;
	}

	std::shared_ptr<IRenderingPipeline> renderingPipeline;
	{
		std::lock_guard lock(context->renderingPipelineMutex_);
		renderingPipeline = context->renderingPipeline_;
	}
	if (renderingPipeline) {
		renderingPipeline->videoRender(effect);
	}
}

} // namespace BackgroundRemoval
