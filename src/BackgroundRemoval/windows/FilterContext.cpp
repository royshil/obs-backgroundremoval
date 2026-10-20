// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "FilterContext.hpp"

#include <cmath>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

#include <UI/AboutDialogIntegration.hpp>
#include <UpdateConfig/UpdateConfig.hpp>

#include "ModuleText.hpp"
#include "AutoMotionConfigurator.hpp"
#include "PipelineFactory.hpp"

namespace BackgroundRemoval {

namespace {

auto tVersionNotice() -> std::string
{
	std::string versionNotice = "<a href=\"https://github.com/royshil/obs-backgroundremoval/releases\">";
	versionNotice += tBackgroundRemoval();
	versionNotice += " ";
	versionNotice += PLUGIN_VERSION_STR;
	if (const std::optional<std::string> latestVersion = UpdateConfig::getLatestVersion();
	    latestVersion.has_value()) {
		versionNotice += " (";
		if (UpdateConfig::isSemverNewer(latestVersion.value(), PLUGIN_VERSION_STR)) {
			versionNotice += tNewVersionsAvailable();
		} else {
			versionNotice += tUpToDate();
		}
		versionNotice += ")";
	}
	versionNotice += "</a>";
	return versionNotice;
}

} // namespace

const obs_source_info FilterContext::kSourceInfo = {
	.id = "background_removal",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = getName,
	.create = create,
	.destroy = destroy,
	.get_defaults = getDefaults,
	.get_properties = getProperties,
	.update = update,
	.video_tick = videoTick,
	.video_render = videoRender,
};

FilterContext::FilterContext(obs_data_t *settings, obs_source_t *source)
	: source_(source),
	  autoMotionConfigurator_(source)
{
	if (!settings || !source_) {
		throw std::invalid_argument("Invalid Windows background-removal context arguments");
	}
	readSettings(settings);
}

FilterContext::~FilterContext() noexcept
{
	renderingPipeline_.store(nullptr, std::memory_order_release);
}

auto FilterContext::getName(void *) noexcept -> const char *
{
	return tBackgroundRemoval();
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

void FilterContext::readSettings(obs_data_t *settings) noexcept
{
	getDefaults(settings);
	pipelineId_.store(obs_data_get_int(settings, "v2_PipelineId"));
	motionSuppression_.store(obs_data_get_double(settings, "v2_MotionSuppression"));
	maskAdjustment_.store(obs_data_get_double(settings, "v2_MaskAdjustment"));
	blurBgEnabled_.store(obs_data_get_bool(settings, "v2_BlurBg"));
	blurBgFactor_.store(obs_data_get_int(settings, "v2_BlurBg_Factor"));
	blurBgFocusPoint_.store(obs_data_get_double(settings, "v2_BlurBg_FocusPoint"));
	blurBgFocusDepth_.store(obs_data_get_double(settings, "v2_BlurBg_FocusDepth"));
	if (const auto pipeline = renderingPipeline_.load(std::memory_order_acquire)) {
		applyRenderingSettings(*pipeline);
	}
}

void FilterContext::applyRenderingSettings(IRenderingPipeline &pipeline) const noexcept
{
	pipeline.setMotionSuppression(motionSuppression_.load());
	pipeline.setMaskAdjustment(maskAdjustment_.load());
	pipeline.setBlurBgEnabled(blurBgEnabled_.load());
	pipeline.setBlurBgFactor(blurBgFactor_.load());
	pipeline.setBlurBgFocusPoint(blurBgFocusPoint_.load());
	pipeline.setBlurBgFocusDepth(blurBgFocusDepth_.load());
}

auto FilterContext::handleAutoSensitivityClick(obs_properties_t *, obs_property_t *, void *data) -> bool
{
	if (auto *contextPtr = static_cast<std::shared_ptr<FilterContext> *>(data); contextPtr && *contextPtr) {
		const auto context = *contextPtr;
		return context->autoMotionConfigurator_.show(context);
	} else {
		auto configurator = std::make_shared<AutoMotionConfigurator>(nullptr);
		return configurator->show(configurator);
	}
}

auto FilterContext::handleAboutButtonClicked(obs_properties_t *, obs_property_t *, void *) -> bool
{
	static_cast<void>(AboutDialogIntegration::show());
	return false;
}

void FilterContext::getDefaults(obs_data_t *settings) noexcept
{
	obs_data_set_default_int(settings, "v2_PipelineId", 0);
	obs_data_set_default_double(settings, "v2_MotionSuppression", 0.0);
	obs_data_set_default_double(settings, "v2_MaskAdjustment", 0.0);
	obs_data_set_default_bool(settings, "v2_BlurBg", false);
	obs_data_set_default_int(settings, "v2_BlurBg_Factor", 4);
	obs_data_set_default_double(settings, "v2_BlurBg_FocusPoint", 0.1);
	obs_data_set_default_double(settings, "v2_BlurBg_FocusDepth", 0.0);
}

auto FilterContext::getProperties(void *data) noexcept -> obs_properties_t *
{
	obs_properties_t *const p = obs_properties_create();

	obs_properties_add_text(p, "v2_VersionNotice", tVersionNotice().c_str(), OBS_TEXT_INFO);

	obs_property_t *const autoSensitivity = obs_properties_add_button(
		p, "v2_Model_AutoSensitivity", tAutoSetSensitivity(), handleAutoSensitivityClick);
	obs_property_set_long_description(autoSensitivity, tAutoSetSensitivityDescription());

	obs_property_t *const pipeline =
		obs_properties_add_list(p, "v2_PipelineId", tPipeline(), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(pipeline, tPipelineAuto(), 0);
	obs_property_list_add_int(pipeline, tPipelineMPLD10(), kMPLD10PipelineId);
	obs_property_set_long_description(pipeline, tPipelineDescription());

	obs_property_t *const maskAdjustment =
		obs_properties_add_float_slider(p, "v2_MaskAdjustment", tMaskAdjustment(), -50.0, 50.0, 0.01);
	obs_property_set_long_description(maskAdjustment, tMaskAdjustmentDescription());

	obs_property_t *const motionSuppression =
		obs_properties_add_float_slider(p, "v2_MotionSuppression", tMotionSuppression(), -50.0, 50.0, 0.01);
	obs_property_set_long_description(motionSuppression, tMotionSuppressionDescription());

	obs_properties_t *const blurProperties = obs_properties_create();
	obs_properties_add_int_slider(blurProperties, "v2_BlurBg_Factor", tBlurBgStrength(), 0, 20, 1);
	obs_properties_add_float_slider(blurProperties, "v2_BlurBg_FocusPoint", tBlurBgFocusPoint(), 0.0, 1.0, 0.05);
	obs_properties_add_float_slider(blurProperties, "v2_BlurBg_FocusDepth", tBlurBgFocusDepth(), 0.0, 0.3, 0.02);
	obs_properties_add_group(p, "v2_BlurBg", tBlurBg(), OBS_GROUP_CHECKABLE, blurProperties);

	return p;
}

void FilterContext::update(void *data, obs_data_t *settings) noexcept
{
	auto *contextPtr = static_cast<std::shared_ptr<FilterContext> *>(data);
	if (!contextPtr || !*contextPtr || !settings) {
		return;
	}
	const std::shared_ptr<FilterContext> context = *contextPtr;
	context->readSettings(settings);
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

	auto pipeline = context->renderingPipeline_.load(std::memory_order_acquire);
	const long long pipelineId = context->pipelineId_.load();
	if (!pipeline || context->activePipelineSelection_ != pipelineId || pipeline->width() != width ||
	    pipeline->height() != height || pipeline->needsRecreation()) {
		pipeline.reset();
		try {
			pipeline = createRenderingPipeline(pipelineId, context->source_, width, height);
			if (pipeline) {
				context->applyRenderingSettings(*pipeline);
				context->activePipelineSelection_ = pipelineId;
			}
		} catch (const std::exception &exception) {
			blog(LOG_ERROR, OBS_LOG_HEADER "Failed to recreate DirectML rendering pipeline: %s",
			     exception.what());
		}
		context->renderingPipeline_.store(pipeline, std::memory_order_release);
	}
	if (pipeline) {
		// Reconcile updates that raced with creation/publication, and apply the latest values
		// before ticking even when update() ran concurrently with the previous application.
		context->applyRenderingSettings(*pipeline);
		pipeline->videoTick(seconds);
		if (const auto motion = pipeline->getMotion(); std::isfinite(motion)) {
			context->autoMotionConfigurator_.recordMotion(motion);
		}
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
	const auto pipeline = context->renderingPipeline_.load(std::memory_order_acquire);
	if (pipeline) {
		pipeline->videoRender(effect);
	}
}

} // namespace BackgroundRemoval
