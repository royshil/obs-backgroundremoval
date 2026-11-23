// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <cstdint>

namespace BackgroundRemoval {

/// The value of the v2_PipelineId setting that selects the recommended pipeline.
inline constexpr std::int64_t kAutoPipelineId = 0;

/// MediaPipe Selfie Segmenter (landscape) on OBS pixel shaders; see MPLSPipeline.
inline constexpr std::int64_t kMPLSPipelineId = 17;

/// The values of the v2_OutputDownscaleFactor setting: the filter outputs 1 / factor of its
/// target's size. A pipeline processes the frame at the target's size and only draws its output
/// smaller.
inline constexpr std::int64_t kOutputDownscaleFactors[] = {1, 2, 4};

/// Returns the setting value if it is one of kOutputDownscaleFactors, otherwise 1.
constexpr auto validOutputDownscaleFactor(std::int64_t factor) noexcept -> std::int64_t
{
	for (const std::int64_t valid : kOutputDownscaleFactors) {
		if (factor == valid) {
			return factor;
		}
	}
	return 1;
}

/// The output size of one side of the target for the downscale factor, which every pipeline
/// reports as its width() and height(): halved once per factor of two, rounded up and at least 1;
/// 0 stays 0.
constexpr auto downscaledSize(std::uint32_t size, std::int64_t factor) noexcept -> std::uint32_t
{
	if (size == 0) {
		return 0;
	}
	for (std::int64_t remaining = validOutputDownscaleFactor(factor); remaining > 1; remaining /= 2) {
		size = std::max(1u, (size + 1) / 2);
	}
	return size;
}

/// A background-removal pipeline owned by a filter instance.
///
/// Every member is called on the OBS graphics thread inside the graphics context.
class IRenderingPipeline {
public:
	virtual ~IRenderingPipeline() noexcept = default;

	/// Returns the value of the v2_PipelineId setting that selects this pipeline.
	virtual auto pipelineId() const noexcept -> std::int64_t = 0;

	/// Returns the width of the output, fixed when the pipeline is created.
	virtual auto width() const noexcept -> std::uint32_t = 0;

	/// Returns the height of the output, fixed when the pipeline is created.
	virtual auto height() const noexcept -> std::uint32_t = 0;

	// MARK: - Blur background parameters

	/// Enables background blur instead of background transparency; default false.
	virtual void setBlurBgEnabled(bool value) noexcept = 0;

	/// Sets the blur strength in [0, 20], default 4; zero leaves the frame sharp. The strength is a
	/// quantity of this filter, not a physical aperture: the blur farthest from the focus point.
	virtual void setBlurBgStrength(double value) noexcept = 0;

	/// Sets where the frame is sharp in [0, 1], default 0.1: 0 focuses on the person and 1 on the
	/// background. A position in mask space, not a physical distance.
	virtual void setBlurBgFocusPoint(double value) noexcept = 0;

	// MARK: - OBS lifecycle handlers

	/// Renders the filtered source frame to the current render target at width() x height(), or
	/// draws nothing when it cannot, so that the unfiltered background is never shown.
	virtual void videoRender() noexcept = 0;
};

} // namespace BackgroundRemoval
