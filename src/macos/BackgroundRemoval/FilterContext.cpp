// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "FilterContext.hpp"

#include <cstdint>
#include <cstring>
#include <exception>
#include <optional>
#include <stdexcept>
#include <string>

#include <ObsBridgeUtils/GsUnique.hpp>
#include "IRenderingPipeline.hpp"
#include "MediaPipeLandscapePipeline.hpp"

#include <UI/AboutDialogIntegration.hpp>
#include <UpdateConfig/UpdateConfig.hpp>
#include <plugin-support.h>

namespace BackgroundRemoval {

auto FilterContext::osLogger() noexcept -> os_log_t
{
	static os_log_t logger = os_log_create("com.royshil.obs-backgroundremoval", "FilterContext");
	return logger;
}

FilterContext::FilterContext(obs_data_t *settings, obs_source_t *source)
	: source_(source),
	  settingsData_(obs_data_create(), obs_data_release)
{
	if (!settings) {
		os_log_with_type(osLogger(), OS_LOG_TYPE_ERROR, "Background removal settings are null");
		throw std::invalid_argument("Background removal settings are null");
	} else if (!source_) {
		os_log_with_type(osLogger(), OS_LOG_TYPE_ERROR, "Background removal source is null");
		throw std::invalid_argument("Background removal source is null");
	} else if (!settingsData_) {
		os_log_with_type(osLogger(), OS_LOG_TYPE_ERROR, "Background removal settings snapshot is null");
		throw std::invalid_argument("Background removal settings snapshot is null");
	}

	getDefaults(settingsData_.get());
	obs_data_apply(settingsData_.get(), settings);
}

FilterContext::~FilterContext() noexcept = default;

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
	using namespace ObsBridgeUtils;

	delete static_cast<std::shared_ptr<FilterContext> *>(data);

	obs_enter_graphics();
	GsUnique::drain();
	obs_leave_graphics();
}

void FilterContext::getDefaults(obs_data_t *settings) noexcept
{
	obs_data_set_default_string(settings, "v2_Model_MatteName", "MediaPipeLandscape");
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
	obs_property_t *const model = obs_properties_add_list(modelProps, "v2_Model_MatteName", "Bg. Rm.",
							      OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(model, obs_module_text("MediaPipe Selfie (Landscape)"), "MediaPipeLandscape");
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

	std::lock_guard lock(context->renderingPipelineMutex_);
	const char *oldModelSelection = obs_data_get_string(context->settingsData_.get(), "v2_Model_MatteName");
	const char *newModelSelection = obs_data_get_string(settings, "v2_Model_MatteName");
	if (strcmp(oldModelSelection, newModelSelection) != 0) {
		context->renderingPipeline_.reset();
	} else if (context->renderingPipeline_) {
		context->renderingPipeline_->update(settings);
	}
	obs_data_clear(context->settingsData_.get());
	obs_data_apply(context->settingsData_.get(), settings);
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
	std::shared_ptr<IRenderingPipeline> renderingPipeline;
	{
		std::lock_guard lock(context->renderingPipelineMutex_);
		if (targetWidth == 0 || targetHeight == 0) {
			context->renderingPipeline_.reset();
			return;
		}

		renderingPipeline = context->renderingPipeline_;
		if (!renderingPipeline || renderingPipeline->getWidth() != targetWidth ||
		    renderingPipeline->getHeight() != targetHeight) {
			try {
				renderingPipeline = std::make_shared<MediaPipeLandscapePipeline>(
					context->settingsData_.get(), context->source_, targetWidth, targetHeight);
			} catch (const std::exception &exception) {
				blog(LOG_ERROR, OBS_LOG_HEADER "Failed to create rendering context: %s",
				     exception.what());
				os_log_with_type(osLogger(), OS_LOG_TYPE_ERROR,
						 "Failed to create rendering context: %{public}s", exception.what());
				return;
			}
			context->renderingPipeline_ = renderingPipeline;
		}
	}

	renderingPipeline->videoTick(seconds);
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

	if (auto renderingPipeline = context->getRenderingPipeline()) {
		renderingPipeline->videoRender(effect);
	}
}

auto FilterContext::getRenderingPipeline() const noexcept -> std::shared_ptr<IRenderingPipeline>
{
	std::lock_guard lock(renderingPipelineMutex_);
	return renderingPipeline_;
}

} // namespace BackgroundRemoval
