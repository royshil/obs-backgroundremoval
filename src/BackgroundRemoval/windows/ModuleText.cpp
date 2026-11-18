// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ModuleText.hpp"

#include <obs-module.h>

namespace BackgroundRemoval {

const char *tBackgroundRemoval()
{
	return obs_module_text("BackgroundRemoval");
}

const char *tUpToDate()
{
	return obs_module_text("Up to date");
}

const char *tNewVersionsAvailable()
{
	return obs_module_text("New Version Available!");
}

const char *tGeneral()
{
	return obs_module_text("General");
}

const char *tMaskAdjustment()
{
	return obs_module_text("Mask adjustment");
}

const char *tMaskAdjustmentDescription()
{
	return obs_module_text("Higher values expand the retained foreground mask. Lower values shrink it. "
			       "The default is 0.");
}

const char *tMotionSuppression()
{
	return obs_module_text("Motion suppression");
}

const char *tMotionSuppressionDescription()
{
	return obs_module_text("Higher values keep the mask steadier by ignoring small changes. "
			       "Lower values make the mask more responsive. The default is 0.");
}

const char *tBlurBg()
{
	return obs_module_text("Blur Background");
}

const char *tBlurBgStrength()
{
	return obs_module_text("Strength");
}

const char *tBlurBgFocusPoint()
{
	return obs_module_text("Focus point");
}

const char *tBlurBgFocusDepth()
{
	return obs_module_text("Focus depth");
}

auto tAboutThisPlugin() -> const char *
{
	return obs_module_text("About this plugin");
}

} // namespace BackgroundRemoval
