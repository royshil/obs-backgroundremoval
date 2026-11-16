// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "FilterContext.hpp"

#include <exception>
#include <optional>
#include <stdexcept>
#include <string>

#include <UI/AboutDialogIntegration.hpp>
#include <UpdateConfig/UpdateConfig.hpp>

#include "ModuleText.hpp"

#ifndef PLUGIN_VERSION_STR
#define PLUGIN_VERSION_STR "0.0.0"
#endif

#ifndef OBS_LOG_HEADER
#define OBS_LOG_HEADER "[obs-backgroundremoval] "
#endif

namespace BackgroundRemoval {

const obs_source_info FilterContext::kSourceInfo = {
	.id = "background_removal",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = getName,
	.create = create,
	.destroy = destroy,
	.get_defaults = getDefaults,
	.get_properties = getProperties,
	.video_render = videoRender,
};

FilterContext::FilterContext(obs_data_t *settings, obs_source_t *source) : source_(source)
{
	if (!settings || !source_) {
		throw std::invalid_argument("Invalid Windows background-removal context arguments");
	}
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
		blog(LOG_ERROR, OBS_LOG_HEADER "Failed to create Windows FilterContext: %s", exception.what());
		return nullptr;
	} catch (...) {
		blog(LOG_ERROR, OBS_LOG_HEADER "Failed to create Windows FilterContext: Unknown error");
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

void FilterContext::getDefaults(obs_data_t *settings) noexcept
{
	obs_data_set_default_double(settings, "v2_MotionSuppression", 0.0);
	obs_data_set_default_double(settings, "v2_MaskAdjustment", 0.0);
	obs_data_set_default_bool(settings, "v2_BlurBg", false);
	obs_data_set_default_int(settings, "v2_BlurBg_Factor", 4);
	obs_data_set_default_double(settings, "v2_BlurBg_FocusPoint", 0.1);
	obs_data_set_default_double(settings, "v2_BlurBg_FocusDepth", 0.0);
}

std::string FilterContext::getVersionNotice()
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

obs_properties_t *FilterContext::getProperties(void *data) noexcept
{
	obs_properties_t *const p = obs_properties_create();

	// MARK: - Header

	try {
		std::string versionNotice = getVersionNotice();
		obs_properties_add_text(p, "v2_VersionNotice", versionNotice.c_str(), OBS_TEXT_INFO);
	} catch (const std::exception &exception) {
		blog(LOG_ERROR, OBS_LOG_HEADER "Failed to get version info: %s", exception.what());
		obs_properties_add_text(p, "v2_VersionNotice", tBackgroundRemoval(), OBS_TEXT_INFO);
	} catch (...) {
		blog(LOG_ERROR, OBS_LOG_HEADER "Failed to get version info: Unknown error");
		obs_properties_add_text(p, "v2_VersionNotice", tBackgroundRemoval(), OBS_TEXT_INFO);
	}

	// MARK: - General Properties

	obs_properties_t *const pGeneral = obs_properties_create();
	obs_properties_add_group(p, "v2_GeneralGroup", tGeneral(), OBS_GROUP_NORMAL, pGeneral);

	obs_property_t *const maskAdjustment =
		obs_properties_add_float_slider(pGeneral, "v2_MaskAdjustment", tMaskAdjustment(), -50.0, 50.0, 0.01);
	obs_property_set_long_description(maskAdjustment, tMaskAdjustmentDescription());

	obs_property_t *const motionSuppression = obs_properties_add_float_slider(
		pGeneral, "v2_MotionSuppression", tMotionSuppression(), -50.0, 50.0, 0.01);
	obs_property_set_long_description(motionSuppression, tMotionSuppressionDescription());

	// MARK: - Blur Background Properties

	obs_properties_t *const blurProperties = obs_properties_create();
	obs_properties_add_group(p, "v2_BlurBg", tBlurBg(), OBS_GROUP_CHECKABLE, blurProperties);

	obs_properties_add_int_slider(blurProperties, "v2_BlurBg_Factor", tBlurBgStrength(), 0, 20, 1);
	obs_properties_add_float_slider(blurProperties, "v2_BlurBg_FocusPoint", tBlurBgFocusPoint(), 0.0, 1.0, 0.05);
	obs_properties_add_float_slider(blurProperties, "v2_BlurBg_FocusDepth", tBlurBgFocusDepth(), 0.0, 0.3, 0.02);

	// MARK: - Footer

	obs_properties_add_button2(p, "v2_AboutButton", tAboutThisPlugin(), handleAboutButtonClicked, data);

	return p;
}

void FilterContext::videoRender(void *data, gs_effect_t *) noexcept
{
	if (auto *context = static_cast<FilterContext *>(data)) {
		obs_source_skip_video_filter(context->source_);
	}
}

} // namespace BackgroundRemoval
