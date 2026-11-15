// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <memory>

#include <obs.h>

namespace BackgroundRemoval {

class MainEffect final {
public:
	MainEffect();
	MainEffect(std::unique_ptr<gs_effect_t, void (*)(gs_effect_t *)> effect);
	~MainEffect() noexcept;

	MainEffect(const MainEffect &) = delete;
	MainEffect(MainEffect &&) = delete;
	auto operator=(const MainEffect &) -> MainEffect & = delete;
	auto operator=(MainEffect &&) -> MainEffect & = delete;

	void destroy() noexcept { effect_.reset(); }

	void copy(gs_texture_t *targetTexture, gs_texture_t *sourceTexture) const noexcept;
	void renderSource(gs_texture_t *targetTexture, obs_source_t *source) const noexcept;
	void lumaAbsDiff(gs_texture_t *targetTexture, gs_texture_t *leftTexture,
			 gs_texture_t *rightTexture) const noexcept;

	void extractScaledRGBPlanes(gs_texture_t *targetTexture, gs_texture_t *sourceTexture) const noexcept;
	void focalBlur(gs_texture_t *targetTexture, gs_texture_t *sourceTexture, gs_texture_t *focalMaskTexture,
		       std::int64_t iteration, std::int64_t total, float focusPoint, float focusDepth) const noexcept;
	void directDrawWithBlurredBackground(gs_texture_t *sourceTexture, gs_texture_t *alphaMaskTexture,
					     gs_texture_t *blurredBackgroundTexture) const noexcept;
	void directDrawWithoutBlur(gs_texture_t *sourceTexture, gs_texture_t *alphaMaskTexture) const noexcept;

	static auto create() -> std::unique_ptr<MainEffect>;

private:
	std::unique_ptr<gs_effect_t, void (*)(gs_effect_t *)> effect_;
	gs_eparam_t *const image_;
	gs_eparam_t *const image1_;
	gs_eparam_t *const alphaMask_;
	gs_eparam_t *const blurredBackground_;
	gs_eparam_t *const focalMask_;
	gs_eparam_t *const xOffset_;
	gs_eparam_t *const yOffset_;
	gs_eparam_t *const blurIteration_;
	gs_eparam_t *const blurTotal_;
	gs_eparam_t *const blurFocusPoint_;
	gs_eparam_t *const blurFocusDepth_;
};

} // namespace BackgroundRemoval
