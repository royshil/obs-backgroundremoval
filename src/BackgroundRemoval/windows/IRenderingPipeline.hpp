// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

struct gs_effect_t;

namespace BackgroundRemoval {

inline constexpr std::int32_t kMPLD10PipelineId = 62828;

/**
 * @brief Windows background-removal rendering contract.
 *
 * Lifecycle calls and recreation checks belong to the OBS video thread. Parameter
 * setters may run concurrently with rendering; multiple setters are not a single
 * atomic settings transaction. Implementations must not propagate exceptions.
 */
class IRenderingPipeline {
public:
	virtual ~IRenderingPipeline() noexcept = default;

	// MARK: - Pipeline properties

	/// Returns the concrete pipeline ID.
	virtual auto pipelineId() const noexcept -> std::int32_t = 0;

	/// Returns the output width provided from the constructor.
	virtual auto width() const noexcept -> std::uint32_t = 0;

	/// Returns the output height provided from the constructor.
	virtual auto height() const noexcept -> std::uint32_t = 0;

	/// Reports whether the owner must replace this instance on the next video tick.
	virtual auto needsRecreation() const noexcept -> bool = 0;

	/// Returns the normalized motion value produced by the latest video tick.
	/// @return A motion value in [0.0, 1.0], or NaN.
	virtual auto getMotion() const noexcept -> double = 0;

	// MARK: - General parameters

	/// Sets motion suppression degree; higher values steady the mask.
	/// @param degree [-50.0, 50.0]; 0.0 is neutral.
	virtual void setMotionSuppression(double degree) noexcept = 0;

	/// Sets mask adjustment degree; positive values expand the foreground.
	/// @param degree [-50.0, 50.0]; 0.0 is neutral.
	virtual void setMaskAdjustment(double degree) noexcept = 0;

	// MARK: - Blur background parameters

	/// Enables background blur instead of background transparency; default false.
	virtual void setBlurBgEnabled(bool value) noexcept = 0;

	/// Sets blur iteration count, default 4; zero disables blur processing.
	virtual void setBlurBgFactor(std::int64_t value) noexcept = 0;

	/// Sets the unblurred mask-space focus point in [0, 1], default 0.1, not physical depth.
	virtual void setBlurBgFocusPoint(double value) noexcept = 0;

	/// Sets the unblurred band around the focus point in mask space, default 0.
	virtual void setBlurBgFocusDepth(double value) noexcept = 0;

	// MARK: - OBS Lifecycle Handlers

	virtual void videoTick(float seconds) noexcept = 0;
	virtual void videoRender(gs_effect_t *effect) noexcept = 0;
};

} // namespace BackgroundRemoval
