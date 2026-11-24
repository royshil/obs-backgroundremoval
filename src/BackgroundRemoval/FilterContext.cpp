// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "FilterContext.hpp"

#include <algorithm>
#include <exception>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <BackgroundRemoval/IRenderingPipeline.hpp>
#include <BackgroundRemoval/MediaPipeLandscapeShader/MPLSPipeline.hpp>
#include <UI/AboutDialogIntegration.hpp>
#include <UpdateConfig/UpdateConfig.hpp>
#include <plugin-support.h>

#include "ModuleText.hpp"
#include "PipelineFactory.hpp"

#ifndef PLUGIN_VERSION_STR
#error PLUGIN_VERSION_STR must be defined by the build system
#endif

namespace {

void appendHtmlEscaped(std::string &output, std::string_view text)
{
	for (const char character : text) {
		switch (character) {
		case '&':
			output += "&amp;";
			break;
		case '<':
			output += "&lt;";
			break;
		case '>':
			output += "&gt;";
			break;
		case '"':
			output += "&quot;";
			break;
		default:
			output += character;
			break;
		}
	}
}

/// The focus ratio in percent. Settings saved before v2_BlurBg_FocusRatio keep the focus point in
/// [0, 1] in v2_BlurBg_FocusPoint; it is read as the ratio that the MPLS pipeline, whose mask those
/// settings were made with, places at the same point. The old key is read only when the new one was
/// never set.
auto readFocusRatio(obs_data_t *settings) -> double
{
	using BackgroundRemoval::MPLSPipeline;
	if (!obs_data_has_user_value(settings, "v2_BlurBg_FocusRatio") &&
	    obs_data_has_user_value(settings, "v2_BlurBg_FocusPoint")) {
		const double point = std::clamp(obs_data_get_double(settings, "v2_BlurBg_FocusPoint"), 0.0, 1.0);
		return MPLSPipeline::focusRatioFromPoint(point);
	}
	return obs_data_get_double(settings, "v2_BlurBg_FocusRatio");
}

} // namespace

namespace BackgroundRemoval {

FilterContext::FilterContext(obs_data_t *settings, obs_source_t *source) : source_(source)
{
	if (!settings || !source_) {
		throw std::invalid_argument("Invalid background-removal context arguments");
	}
	readSettings(settings);
}

FilterContext::~FilterContext() noexcept
{
	obs_enter_graphics();
	pipeline_.reset();
	obs_leave_graphics();
}

const char *FilterContext::getName(void *) noexcept
{
	return tBackgroundRemoval();
}

void *FilterContext::create(obs_data_t *settings, obs_source_t *source) noexcept
{
	try {
		return new FilterContext(settings, source);
	} catch (const std::exception &exception) {
		obs_log(LOG_ERROR, "Failed to create FilterContext: %s", exception.what());
		return nullptr;
	} catch (...) {
		obs_log(LOG_ERROR, "Failed to create FilterContext: Unknown error");
		return nullptr;
	}
}

void FilterContext::destroy(void *data) noexcept
{
	delete static_cast<FilterContext *>(data);
}

auto FilterContext::handleAboutButtonClicked(obs_properties_t *, obs_property_t *, void *) -> bool
{
	static_cast<void>(AboutDialogIntegration::show());
	return false;
}

void FilterContext::update(void *data, obs_data_t *settings) noexcept
{
	if (auto *context = static_cast<FilterContext *>(data)) {
		context->readSettings(settings);
	}
}

void FilterContext::readSettings(obs_data_t *settings) noexcept
{
	pipelineId_.store(obs_data_get_int(settings, "v2_PipelineId"));
	blurBgEnabled_.store(obs_data_get_bool(settings, "v2_BlurBg"));
	// v2_BlurBg_Factor was an integer iteration count; obs_data reads stored integers as doubles.
	blurBgStrength_.store(obs_data_get_double(settings, "v2_BlurBg_Factor"));
	blurBgFocusRatio_.store(readFocusRatio(settings));
}

void FilterContext::applySettings(IRenderingPipeline &pipeline) const noexcept
{
	pipeline.setBlurBgEnabled(blurBgEnabled_.load());
	pipeline.setBlurBgStrength(blurBgStrength_.load());
	pipeline.setBlurBgFocusRatio(blurBgFocusRatio_.load());
}

void FilterContext::getDefaults(obs_data_t *settings) noexcept
{
	obs_data_set_default_int(settings, "v2_PipelineId", kAutoPipelineId);
	obs_data_set_default_bool(settings, "v2_BlurBg", false);
	obs_data_set_default_double(settings, "v2_BlurBg_Factor", 4.0);
	obs_data_set_default_int(settings, "v2_BlurBg_FocusRatio", -100);
}

std::string FilterContext::getVersionNotice()
{
	std::string versionNotice = "<a href=\"https://github.com/royshil/obs-backgroundremoval/releases\">";
	appendHtmlEscaped(versionNotice, tBackgroundRemoval());
	versionNotice += " ";
	appendHtmlEscaped(versionNotice, PLUGIN_VERSION_STR);
	if (const std::optional<std::string> latestVersion = UpdateConfig::getLatestVersion();
	    latestVersion.has_value()) {
		versionNotice += " (";
		if (UpdateConfig::isSemverNewer(latestVersion.value(), PLUGIN_VERSION_STR)) {
			appendHtmlEscaped(versionNotice, tNewVersionsAvailable());
		} else {
			appendHtmlEscaped(versionNotice, tUpToDate());
		}
		versionNotice += ")";
	}
	versionNotice += "</a>";
	return versionNotice;
}

obs_properties_t *FilterContext::getProperties(void *data) noexcept
{
	obs_properties_t *const p = obs_properties_create();

	// MARK: - Header

	try {
		std::string versionNotice = getVersionNotice();
		obs_properties_add_text(p, "v2_VersionNotice", versionNotice.c_str(), OBS_TEXT_INFO);
	} catch (const std::exception &exception) {
		obs_log(LOG_ERROR, "Failed to get version info: %s", exception.what());
		obs_properties_add_text(p, "v2_VersionNotice", tBackgroundRemoval(), OBS_TEXT_INFO);
	} catch (...) {
		obs_log(LOG_ERROR, "Failed to get version info: Unknown error");
		obs_properties_add_text(p, "v2_VersionNotice", tBackgroundRemoval(), OBS_TEXT_INFO);
	}

	// MARK: - General Properties

	obs_properties_t *const pGeneral = obs_properties_create();
	obs_properties_add_group(p, "v2_GeneralGroup", tGeneral(), OBS_GROUP_NORMAL, pGeneral);

	obs_property_t *const pipeline = obs_properties_add_list(pGeneral, "v2_PipelineId", tPipeline(),
								 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(pipeline, tPipelineAuto(), kAutoPipelineId);
	obs_property_list_add_int(pipeline, tPipelineMPLS(), kMPLSPipelineId);
	obs_property_set_long_description(pipeline, tPipelineDescription());

	// MARK: - Blur Background Properties

	obs_properties_t *const blurProperties = obs_properties_create();
	obs_properties_add_group(p, "v2_BlurBg", tBlurBg(), OBS_GROUP_CHECKABLE, blurProperties);

	obs_property_t *const blurStrength =
		obs_properties_add_float_slider(blurProperties, "v2_BlurBg_Factor", tBlurBgStrength(), 0.0, 20.0, 0.1);
	obs_property_set_long_description(blurStrength, tBlurBgStrengthDescription());
	obs_property_t *const blurFocusRatio = obs_properties_add_int_slider(blurProperties, "v2_BlurBg_FocusRatio",
									     tBlurBgFocusRatio(), -100, 100, 1);
	obs_property_int_set_suffix(blurFocusRatio, "%");
	obs_property_set_long_description(blurFocusRatio, tBlurBgFocusRatioDescription());

	// MARK: - Footer

	obs_properties_add_button2(p, "v2_AboutButton", tAboutThisPlugin(), handleAboutButtonClicked, data);

	return p;
}

// The output size is the one of the current pipeline, as video_tick created it, or 0 without one.
// These callbacks query no source size: for the parent source, obs_source_get_width/height() return
// the size of its first filter, which may be this filter, under the parent's filter mutex.
std::uint32_t FilterContext::getWidth(void *data) noexcept
{
	const auto *context = static_cast<FilterContext *>(data);
	return context ? context->outputWidth_.load() : 0;
}

std::uint32_t FilterContext::getHeight(void *data) noexcept
{
	const auto *context = static_cast<FilterContext *>(data);
	return context ? context->outputHeight_.load() : 0;
}

void FilterContext::videoTick(void *data, float) noexcept
{
	if (auto *context = static_cast<FilterContext *>(data)) {
		context->tick();
	}
}

// Recreates the pipeline whenever the selection or the target's size changes, as
// live-backgroundremoval-lite does, with the target's base size. It works while the parent source is
// showing anywhere, not only while it is active in the program output, so that the Studio Mode
// preview, the filter properties preview and projectors show the source with the background
// removed. A key that failed is not retried until something changes again. video_tick runs on the
// graphics thread outside the graphics context, so enter it to create or destroy the pipeline.
void FilterContext::tick() noexcept
{
	obs_source_t *const parent = obs_filter_get_parent(source_);
	obs_source_t *const target = obs_filter_get_target(source_);
	if (!parent || !obs_source_showing(parent) || !target) {
		return;
	}
	const PipelineKey key{
		.pipelineId = pipelineId_.load(),
		.sourceWidth = obs_source_get_base_width(target),
		.sourceHeight = obs_source_get_base_height(target),
	};
	if (key.sourceWidth == 0 || key.sourceHeight == 0) {
		resetPipeline();
		return;
	}
	if (pipelineKey_ == key) {
		return;
	}
	pipelineKey_ = key;

	obs_enter_graphics();
	pipeline_.reset();
	try {
		pipeline_ = createRenderingPipeline(key.pipelineId, source_, key.sourceWidth, key.sourceHeight);
	} catch (const std::exception &exception) {
		obs_log(LOG_ERROR, "Failed to create the rendering pipeline: %s", exception.what());
	} catch (...) {
		obs_log(LOG_ERROR, "Failed to create the rendering pipeline: Unknown error");
	}
	obs_leave_graphics();
	outputWidth_.store(pipeline_ ? pipeline_->width() : 0);
	outputHeight_.store(pipeline_ ? pipeline_->height() : 0);
}

void FilterContext::resetPipeline() noexcept
{
	if (pipeline_) {
		obs_enter_graphics();
		pipeline_.reset();
		obs_leave_graphics();
	}
	pipelineKey_.reset();
	outputWidth_.store(0);
	outputHeight_.store(0);
}

void FilterContext::videoRender(void *data, gs_effect_t *) noexcept
{
	if (auto *context = static_cast<FilterContext *>(data)) {
		context->render();
	}
}

// Draws nothing when the parent is not shown or there is no pipeline, as live-backgroundremoval-lite
// does, so that the unfiltered background is never disclosed.
void FilterContext::render() noexcept
{
	obs_source_t *const parent = obs_filter_get_parent(source_);
	if (!parent || !obs_source_showing(parent) || !pipeline_) {
		return;
	}
	applySettings(*pipeline_);
	pipeline_->videoRender();
}

} // namespace BackgroundRemoval
