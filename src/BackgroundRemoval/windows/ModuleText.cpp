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

auto tShowFilterWizard() -> const char *
{
	return obs_module_text("Show auto-configuration wizard for this filter...");
}

auto tGeneral() -> const char *
{
	return obs_module_text("General");
}

auto tPipeline() -> const char *
{
	return obs_module_text("Pipeline");
}

auto tPipelineAuto() -> const char *
{
	return obs_module_text("Auto (MediaPipe)");
}

auto tPipelineMPLD10() -> const char *
{
	return obs_module_text("MediaPipe (Landscape) - DirectML (Baseline)");
}

auto tPipelineDescription() -> const char *
{
	return obs_module_text(
		"Auto selects the recommended pipeline. Select a specific pipeline to use it explicitly.");
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

auto tAutoSetSensitivityDescription() -> const char *
{
	return obs_module_text(
		"Open a dialog to measure small changes, preview the result, and adjust motion suppression.");
}

auto tAutoMotionTitle() -> const char *
{
	return obs_module_text("Automatic motion suppression");
}

auto tAutoMotionInstructions() -> const char *
{
	return obs_module_text(
		"Keep still during the three-second measurement. Small changes in lighting and camera noise "
		"will be used to adjust motion suppression. After measuring, move normally to check the preview.");
}

auto tStartMotionMeasurement() -> const char *
{
	return obs_module_text("Start measurement");
}

auto tMeasureMotionAgain() -> const char *
{
	return obs_module_text("Measure again");
}

auto tMeasuringMotion() -> const char *
{
	return obs_module_text("Measuring... Keep still.");
}

auto tNoMotionInput() -> const char *
{
	return obs_module_text(
		"No video input available. Make sure the filter is active and video is playing, then try again.");
}

auto tMotionMeasurementResult() -> const char *
{
	return obs_module_text("Recommended motion suppression: %1 (%2 samples).");
}

auto tBlurBg() -> const char *
{
	return obs_module_text("Blur Bg.");
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

auto tRefineMask() -> const char *
{
	return obs_module_text("Refine Mask");
}

} // namespace BackgroundRemoval
