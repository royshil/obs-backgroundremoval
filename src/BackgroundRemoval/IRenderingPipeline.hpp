// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

namespace BackgroundRemoval {

/// The value of the v2_PipelineId setting that selects the recommended pipeline.
inline constexpr std::int64_t kAutoPipelineId = 0;

/// MediaPipe Selfie Segmenter (landscape) on OBS pixel shaders; see MPLSPipeline.
inline constexpr std::int64_t kMPLSPipelineId = 17;

/// A background-removal pipeline owned by a filter instance.
///
/// Every member is called on the OBS graphics thread inside the graphics context.
class IRenderingPipeline {
public:
	virtual ~IRenderingPipeline() noexcept = default;

	/// Returns the value of the v2_PipelineId setting that selects this pipeline.
	virtual auto pipelineId() const noexcept -> std::int64_t = 0;

	/// Returns the width of the output: the width of the target the pipeline was created for.
	virtual auto width() const noexcept -> std::uint32_t = 0;

	/// Returns the height of the output: the height of the target the pipeline was created for.
	virtual auto height() const noexcept -> std::uint32_t = 0;

	// MARK: - Blur background parameters

	/// Enables background blur instead of background transparency; default false.
	virtual void setBlurBgEnabled(bool value) noexcept = 0;

	/// Sets the blur strength in [0, 20], default 4; zero leaves the frame sharp. The strength is a
	/// quantity of this filter, not a physical aperture: the blur farthest from the focus point.
	virtual void setBlurBgStrength(double value) noexcept = 0;

	/// Sets where the frame is sharp as a percentage in [-100, 100], default -100: -100 focuses on
	/// the person, 100 on the background, and 0 is the point where the two balance. A position in
	/// mask space, not a physical distance. Each pipeline places it on its own mask, since where the
	/// person, the background and the balance between them lie there depends on the model.
	virtual void setBlurBgFocusRatio(double percent) noexcept = 0;

	// MARK: - OBS lifecycle handlers

	/// Renders the filtered source frame to the current render target at width() x height(), or
	/// draws nothing when it cannot, so that the unfiltered background is never shown.
	virtual void videoRender() noexcept = 0;
};

} // namespace BackgroundRemoval
