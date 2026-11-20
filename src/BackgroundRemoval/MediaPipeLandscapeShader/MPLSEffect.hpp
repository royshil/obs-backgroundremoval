// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <graphics/graphics.h>

namespace BackgroundRemoval {

/// The effect that evaluates the network, and its texture parameters.
struct MPLSEffect final {
	/// @throws std::runtime_error when the effect lacks a parameter or the Draw technique.
	explicit MPLSEffect(gs_effect_t *effect);

	MPLSEffect(const MPLSEffect &) = delete;
	auto operator=(const MPLSEffect &) -> MPLSEffect & = delete;
	MPLSEffect(MPLSEffect &&) = delete;
	auto operator=(MPLSEffect &&) -> MPLSEffect & = delete;

	gs_effect_t *const effect;
	gs_eparam_t *const epTensor;
	gs_eparam_t *const epScale;
	gs_eparam_t *const epTensorB;
	gs_eparam_t *const epScaleB;
	gs_eparam_t *const epTensorR;
	gs_eparam_t *const epWeights;
	gs_eparam_t *const epBias;
	gs_eparam_t *const epImage;
};

/// The effect that evaluates the network, compiled on the first call inside the graphics context
/// and shared by every pipeline; libobs keeps it until the graphics subsystem is destroyed.
/// @throws std::runtime_error with the compiler's messages when the effect cannot be created, or
/// when it lacks a parameter or the Draw technique.
const MPLSEffect &makeMPLSEffect();

} // namespace BackgroundRemoval
