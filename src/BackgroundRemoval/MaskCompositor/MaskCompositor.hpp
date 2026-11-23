// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <obs.h>

namespace BackgroundRemoval {

/// The effect MaskCompositor draws with, and its parameters.
struct MaskCompositorEffect final {
	/// @throws std::runtime_error when the effect lacks a technique or a parameter.
	explicit MaskCompositorEffect(gs_effect_t *effect);

	MaskCompositorEffect(const MaskCompositorEffect &) = delete;
	auto operator=(const MaskCompositorEffect &) -> MaskCompositorEffect & = delete;
	MaskCompositorEffect(MaskCompositorEffect &&) = delete;
	auto operator=(MaskCompositorEffect &&) -> MaskCompositorEffect & = delete;

	gs_effect_t *const effect;
	gs_eparam_t *const epImage;
	gs_eparam_t *const epBlurred;
	gs_eparam_t *const epMask;
	gs_eparam_t *const epProbability;
	gs_eparam_t *const epThreshold;
	gs_eparam_t *const epMaskWidth;
	gs_eparam_t *const epMaskHeight;
	gs_eparam_t *const epHalfPixel;
	gs_eparam_t *const epStrength;
	gs_eparam_t *const epFocusPoint;
	gs_eparam_t *const epLevelOffset;
	gs_eparam_t *const epLevelScale;
};

/// The effect MaskCompositor draws with, compiled on the first call inside the graphics context and
/// shared by every compositor; libobs keeps it until the graphics subsystem is destroyed.
/// @throws std::runtime_error with the compiler's messages when the effect cannot be created, or
/// when it lacks a technique or a parameter.
const MaskCompositorEffect &makeMaskCompositorEffect();

/// Draws a source frame with a foreground probability mask, using OBS graphics only.
///
/// The probability is first refined at its own size: each texel becomes the share of the
/// foreground (probability at least kForegroundThreshold) in the 5x5 window around it, counting
/// only the texels inside the mask. The refined mask is sampled bilinearly at the frame size, and
/// a pixel is in the foreground when the share is at least 0.5.
///
/// The background is either removed (made transparent), or the whole frame is drawn opaque with a
/// focal blur: Dual filtering (Bjorge, SIGGRAPH 2015) whose amount at each pixel grows with the
/// distance of the pixel from the focus point in mask space, up to the strength.
///
/// Construct, render and destroy instances only inside the OBS graphics context.
class MaskCompositor final {
public:
	/// The largest blur strength.
	static constexpr float kMaxBlurStrength = 20.0F;

	/// The foreground probability threshold of the refinement, the one the model is trained for.
	static constexpr float kForegroundThreshold = 0.5F;

	/// Draws with the shared effect of makeMaskCompositorEffect().
	/// @throws std::runtime_error when the effect cannot be created.
	MaskCompositor();
	~MaskCompositor() noexcept;

	MaskCompositor(const MaskCompositor &) = delete;
	auto operator=(const MaskCompositor &) -> MaskCompositor & = delete;
	MaskCompositor(MaskCompositor &&) = delete;
	auto operator=(MaskCompositor &&) -> MaskCompositor & = delete;

	/// Blurs instead of removing the background; default false.
	void setBlurEnabled(bool value) noexcept;

	/// Sets the blur strength in [0, kMaxBlurStrength], default 4; zero leaves the frame sharp.
	/// Strength s blurs as much (by variance) as s iterations of the Kawase blur this replaces.
	void setBlurStrength(float value) noexcept;

	/// Sets where the frame is sharp in [0, 1], default 0: 0 focuses on the foreground and 1 on the
	/// background. The blur grows with the distance from it in mask space.
	void setBlurFocusPoint(float value) noexcept;

	/// Refines the probability mask (a texture of any size, the probability in red), and blurs
	/// image of width x height when blur is enabled, into textures of the compositor. Call it every
	/// frame before draw(), while the caller's render state is saved: it changes the render target,
	/// projection, viewport and blend state.
	/// @throws std::runtime_error when a texture cannot be created.
	void prepare(gs_texture_t *image, gs_texture_t *probability, std::uint32_t width, std::uint32_t height);

	/// Draws the composited frame of width x height, with the mask of the latest prepare(), to the
	/// current render target with the current projection and blend state. Draws in linear space, as
	/// OBS draws an sRGB-aware filter: the frame is sampled through sRGB decoding and written with
	/// framebuffer sRGB, so its alpha divides and blends there.
	void draw(gs_texture_t *image, std::uint32_t width, std::uint32_t height) noexcept;

	/// The refined mask of the latest prepare(): the share of the foreground in R8, at the size of
	/// the probability mask. For tests and diagnostics.
	auto refinedMask() const noexcept -> gs_texture_t * { return refinedMask_.get(); }

private:
	struct TextureDeleter {
		void operator()(gs_texture_t *texture) const noexcept { gs_texture_destroy(texture); }
	};
	using UniqueTexture = std::unique_ptr<gs_texture_t, TextureDeleter>;

	/// One level of the blur pyramid: the downsampled level and the upsampled, blended level.
	/// Level 0 is the frame itself and has only up.
	struct BlurLevel {
		UniqueTexture down;
		UniqueTexture up;
		std::uint32_t width = 0;
		std::uint32_t height = 0;
	};

	void ensureMaskTextures(std::uint32_t width, std::uint32_t height);
	void ensurePyramid(std::uint32_t width, std::uint32_t height, std::size_t levels);

	const MaskCompositorEffect &effect_;

	bool blurEnabled_ = false;
	float blurStrength_ = 4.0F;
	float blurFocusPoint_ = 0.0F;

	/// The share of the foreground in R8, at the size of the probability mask.
	UniqueTexture refinedMask_;
	std::uint32_t maskTexturesWidth_ = 0;
	std::uint32_t maskTexturesHeight_ = 0;

	/// The blur pyramid, from level 0 (the frame size) down to the deepest level created so far.
	std::vector<BlurLevel> pyramid_;
	/// The blurred frame of the latest prepare(), or null when draw() removes the background.
	gs_texture_t *blurredFrame_ = nullptr;
};

} // namespace BackgroundRemoval
