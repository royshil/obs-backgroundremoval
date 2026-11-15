// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ModuleText.hpp"

#include <obs-module.h>

namespace BackgroundRemoval {

auto tBackgroundRemoval() -> const char *
{
	return obs_module_text("Background Removal");
}

auto tUpToDate() -> const char *
{
	return obs_module_text("Up to date");
}

auto tNewVersionsAvailable() -> const char *
{
	return obs_module_text("New Version Available!");
}

auto tGeneral() -> const char *
{
	return obs_module_text("General");
}

auto tMaskAdjustment() -> const char *
{
	return obs_module_text("Mask adjustment");
}

auto tMaskAdjustmentDescription() -> const char *
{
	return obs_module_text("Higher values expand the retained foreground mask. Lower values shrink it. "
			       "The default is 0.");
}

auto tMotionSuppression() -> const char *
{
	return obs_module_text("Motion suppression");
}

auto tMotionSuppressionDescription() -> const char *
{
	return obs_module_text("Higher values keep the mask steadier by ignoring small changes. "
			       "Lower values make the mask more responsive. The default is 0.");
}

auto tBlurBg() -> const char *
{
	return obs_module_text("Blur Background");
}

auto tBlurBgStrength() -> const char *
{
	return obs_module_text("Strength");
}

auto tBlurBgFocusPoint() -> const char *
{
	return obs_module_text("Focus point");
}

auto tBlurBgFocusDepth() -> const char *
{
	return obs_module_text("Focus depth");
}

auto tAboutThisPlugin() -> const char *
{
	return obs_module_text("About this plugin");
}

} // namespace BackgroundRemoval
