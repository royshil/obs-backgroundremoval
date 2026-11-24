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

const char *tPipeline()
{
	return obs_module_text("Pipeline");
}

const char *tPipelineAuto()
{
	return obs_module_text("Auto (MediaPipe)");
}

const char *tPipelineMPLS()
{
	return obs_module_text("MediaPipe (Landscape) - Default GPU");
}

const char *tPipelineDescription()
{
	return obs_module_text(
		"Auto selects the recommended pipeline. Select a specific pipeline to use it explicitly.");
}

const char *tBlurBg()
{
	return obs_module_text("Blur Background");
}

const char *tBlurBgStrength()
{
	return obs_module_text("Strength");
}

const char *tBlurBgStrengthDescription()
{
	return obs_module_text("How strongly the parts farthest from the focus point are blurred. "
			       "0 leaves the image sharp. The default is 4.");
}

const char *tBlurBgFocusRatio()
{
	return obs_module_text("Focus ratio");
}

const char *tBlurBgFocusRatioDescription()
{
	return obs_module_text("Where the image stays sharp: -100% keeps the person sharp, 100% keeps the "
			       "background sharp, and 0% balances the two. The blur grows with the distance "
			       "from there. The default is -100%.");
}

auto tAboutThisPlugin() -> const char *
{
	return obs_module_text("About this plugin");
}

} // namespace BackgroundRemoval
