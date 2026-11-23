// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MaskCompositor.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include <graphics/vec2.h>
#include <util/bmem.h>

#include "BlurLevels.hpp"

namespace BackgroundRemoval {

namespace {

// clang-format off
constexpr char kMaskCompositorEffectCode[] = R"EFFECT(
/*
 * Terms
 *
 * Probability
 *   The foreground probability in [0, 1] in the red channel, of maskWidth x maskHeight texels.
 * Mask
 *   The refined probability: the share of the foreground in the 5x5 window around each texel, of
 *   the same size. It is sampled bilinearly at the uv of the fragment.
 * Foreground
 *   Where the mask is at least 0.5. The test is binary, so scaling all four channels by it is
 *   correct for both premultiplied and straight alpha.
 * Level k
 *   Level k of the blur pyramid: level 0 is the frame, and each level is half the size of the one
 *   above it, rounded up.
 */

uniform float4x4 ViewProj;

uniform texture2d image;
uniform texture2d blurred;
uniform texture2d mask;
uniform texture2d probability;
uniform texture2d focus;

uniform float threshold;
uniform int maskWidth;
uniform int maskHeight;
uniform float2 halfPixel;
uniform float strength;
uniform float focusPoint;
uniform float levelOffset;
uniform float levelScale;

sampler_state linearSampler {
	Filter = Linear;
	AddressU = Clamp;
	AddressV = Clamp;
};

struct VertData {
	float4 pos : POSITION;
	float2 uv : TEXCOORD0;
};

VertData VSDefault(VertData v_in)
{
	VertData vert_out;
	vert_out.pos = mul(float4(v_in.pos.xyz, 1.0), ViewProj);
	vert_out.uv = v_in.uv;
	return vert_out;
}

/*
 * The share of the foreground in the 5x5 window around the fragment of the mask, where a texel is
 * foreground when its probability reaches the threshold. Only texels inside the probability are
 * counted, as ONNX AveragePool with count_include_pad = 0.
 * Uniforms: probability, threshold, maskWidth, maskHeight.
 */
float4 PS_RefineMask(VertData v_in) : TARGET
{
	int x = int(v_in.uv.x * float(maskWidth));
	int y = int(v_in.uv.y * float(maskHeight));
	float foreground = 0.0;
	float count = 0.0;
	for (int dy = -2; dy <= 2; dy++) {
		for (int dx = -2; dx <= 2; dx++) {
			int tx = x + dx;
			int ty = y + dy;
			float inside = (tx >= 0 && tx < maskWidth && ty >= 0 && ty < maskHeight) ? 1.0 : 0.0;
			int3 coords = int3(clamp(tx, 0, maskWidth - 1), clamp(ty, 0, maskHeight - 1), 0);
			foreground += step(threshold, probability.Load(coords).r) * inside;
			count += inside;
		}
	}
	float share = foreground / count;
	return float4(share, share, share, 1.0);
}

technique RefineMask { pass { vertex_shader = VSDefault(v_in); pixel_shader = PS_RefineMask(v_in); } }

/*
 * 1 in the foreground, 0 in the background.
 * Uniforms: mask.
 */
float foregroundAt(float2 uv)
{
	return step(0.5, mask.Sample(linearSampler, uv).r);
}

/*
 * The image with the background removed.
 * Uniforms: image, mask.
 */
float4 PS_RemoveBackground(VertData v_in) : TARGET
{
	return image.Sample(linearSampler, v_in.uv) * foregroundAt(v_in.uv);
}

technique RemoveBackground { pass { vertex_shader = VSDefault(v_in); pixel_shader = PS_RemoveBackground(v_in); } }

/*
 * The downsample filter of Dual filtering (Marius Bjorge, "Bandwidth-Efficient Rendering",
 * SIGGRAPH 2015): the center at weight 1/2 and the four corners of the output texel at 1/8 each.
 * halfPixel is half a texel of the output, the lower resolution, in uv.
 * Uniforms: image, halfPixel.
 */
float4 PS_Downsample(VertData v_in) : TARGET
{
	float2 uv = v_in.uv;
	float4 sum = image.Sample(linearSampler, uv) * 4.0;
	sum += image.Sample(linearSampler, uv - halfPixel);
	sum += image.Sample(linearSampler, uv + halfPixel);
	sum += image.Sample(linearSampler, uv + float2(halfPixel.x, -halfPixel.y));
	sum += image.Sample(linearSampler, uv - float2(halfPixel.x, -halfPixel.y));
	return sum / 8.0;
}

technique Downsample { pass { vertex_shader = VSDefault(v_in); pixel_shader = PS_Downsample(v_in); } }

/*
 * The focus distance, at the size of the mask: the distance of the fragment from the focus point
 * in [0, 1], the background share of five taps of the mask against focusPoint (0 focuses on the
 * foreground, 1 on the background). It depends on the mask alone, so it is computed once per frame
 * and the upsample passes sample it.
 * Uniforms: mask, focusPoint.
 */
float4 PS_FocusDistance(VertData v_in) : TARGET
{
	float2 uv = v_in.uv;
	float foreground = foregroundAt(uv);
	foreground += foregroundAt(uv + float2(0.01, 0.01));
	foreground += foregroundAt(uv + float2(-0.01, 0.01));
	foreground += foregroundAt(uv + float2(0.01, -0.01));
	foreground += foregroundAt(uv + float2(-0.01, -0.01));
	float background = 1.0 - foreground * 0.2;
	float focusDistance = clamp(abs(background - focusPoint), 0.0, 1.0);
	return float4(focusDistance, focusDistance, focusDistance, 1.0);
}

technique FocusDistance { pass { vertex_shader = VSDefault(v_in); pixel_shader = PS_FocusDistance(v_in); } }

/*
 * The upsample filter of Dual filtering (Bjorge, SIGGRAPH 2015): four diagonal taps at 1/6 and four
 * axial taps twice as far at 1/12, a ring around the fragment. halfPixel is half a texel of the
 * input, the lower resolution, in uv. The result is blended with the same level before the blur by
 * the weight of this level for the fragment:
 *   t = strength * focus distance, the strength for the fragment;
 *   weight = clamp((t (2 t^2 + 1) - levelOffset) * levelScale, 0, 1).
 * See MaskCompositor::prepare() for the derivation of levelOffset and levelScale.
 * Uniforms: blurred (the lower level), image (this level before the blur), focus (the focus
 * distance), halfPixel, strength, levelOffset, levelScale.
 */
float4 PS_Upsample(VertData v_in) : TARGET
{
	float2 uv = v_in.uv;
	float4 sum = blurred.Sample(linearSampler, uv + float2(-halfPixel.x * 2.0, 0.0));
	sum += blurred.Sample(linearSampler, uv + float2(-halfPixel.x, halfPixel.y)) * 2.0;
	sum += blurred.Sample(linearSampler, uv + float2(0.0, halfPixel.y * 2.0));
	sum += blurred.Sample(linearSampler, uv + float2(halfPixel.x, halfPixel.y)) * 2.0;
	sum += blurred.Sample(linearSampler, uv + float2(halfPixel.x * 2.0, 0.0));
	sum += blurred.Sample(linearSampler, uv + float2(halfPixel.x, -halfPixel.y)) * 2.0;
	sum += blurred.Sample(linearSampler, uv + float2(0.0, -halfPixel.y * 2.0));
	sum += blurred.Sample(linearSampler, uv + float2(-halfPixel.x, -halfPixel.y)) * 2.0;
	float t = strength * focus.Sample(linearSampler, uv).r;
	float weight = clamp((t * (2.0 * t * t + 1.0) - levelOffset) * levelScale, 0.0, 1.0);
	return lerp(image.Sample(linearSampler, uv), sum / 12.0, weight);
}

technique Upsample { pass { vertex_shader = VSDefault(v_in); pixel_shader = PS_Upsample(v_in); } }

/*
 * The blurred image, opaque.
 * Uniforms: blurred.
 */
float4 PS_DrawBlurred(VertData v_in) : TARGET
{
	return float4(blurred.Sample(linearSampler, v_in.uv).rgb, 1.0);
}

technique DrawBlurred { pass { vertex_shader = VSDefault(v_in); pixel_shader = PS_DrawBlurred(v_in); } }
)EFFECT";
// clang-format on

auto requireParam(gs_effect_t *effect, const char *name) -> gs_eparam_t *
{
	gs_eparam_t *const param = gs_effect_get_param_by_name(effect, name);
	if (!param) {
		throw std::runtime_error(std::string("MaskCompositor effect parameter is missing: ") + name);
	}
	return param;
}

// gs_effect_loop() draws nothing when a technique is missing, so check the names up front.
void requireTechnique(gs_effect_t *effect, const char *name)
{
	if (!gs_effect_get_technique(effect, name)) {
		throw std::runtime_error(std::string("MaskCompositor effect technique is missing: ") + name);
	}
}

void setTarget(gs_texture_t *target, std::uint32_t width, std::uint32_t height) noexcept
{
	gs_set_render_target_with_color_space(target, nullptr, GS_CS_SRGB);
	gs_ortho(0.0F, static_cast<float>(width), 0.0F, static_cast<float>(height), -100.0F, 100.0F);
	gs_set_viewport(0, 0, static_cast<int>(width), static_cast<int>(height));
}

auto finiteClamp(float value, float low, float high, float fallback) noexcept -> float
{
	return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
}

} // namespace

MaskCompositorEffect::MaskCompositorEffect(gs_effect_t *effect)
	: effect(effect),
	  epImage(requireParam(effect, "image")),
	  epBlurred(requireParam(effect, "blurred")),
	  epMask(requireParam(effect, "mask")),
	  epFocus(requireParam(effect, "focus")),
	  epProbability(requireParam(effect, "probability")),
	  epThreshold(requireParam(effect, "threshold")),
	  epMaskWidth(requireParam(effect, "maskWidth")),
	  epMaskHeight(requireParam(effect, "maskHeight")),
	  epHalfPixel(requireParam(effect, "halfPixel")),
	  epStrength(requireParam(effect, "strength")),
	  epFocusPoint(requireParam(effect, "focusPoint")),
	  epLevelOffset(requireParam(effect, "levelOffset")),
	  epLevelScale(requireParam(effect, "levelScale"))
{
	for (const char *technique :
	     {"RefineMask", "FocusDistance", "RemoveBackground", "Downsample", "Upsample", "DrawBlurred"}) {
		requireTechnique(effect, technique);
	}
}

const MaskCompositorEffect &makeMaskCompositorEffect()
{
	static char *s_pErrorString = nullptr;
	static gs_effect_t *s_effect =
		gs_effect_create(kMaskCompositorEffectCode, "kMaskCompositorEffectCode", &s_pErrorString);
	static std::unique_ptr<char, void (*)(void *)> s_errorString(s_pErrorString, &bfree);
	if (!s_effect) {
		throw std::runtime_error(s_pErrorString ? s_pErrorString : "Unknown error");
	}
	static const MaskCompositorEffect s_maskCompositorEffect(s_effect);
	return s_maskCompositorEffect;
}

MaskCompositor::MaskCompositor() : effect_(makeMaskCompositorEffect()) {}

MaskCompositor::~MaskCompositor() noexcept = default;

void MaskCompositor::setBlurEnabled(bool value) noexcept
{
	blurEnabled_ = value;
}

void MaskCompositor::setBlurStrength(float value) noexcept
{
	blurStrength_ = finiteClamp(value, 0.0F, kMaxBlurStrength, 4.0F);
}

void MaskCompositor::setBlurFocusPoint(float value) noexcept
{
	blurFocusPoint_ = finiteClamp(value, 0.0F, 1.0F, 0.1F);
}

void MaskCompositor::prepare(gs_texture_t *image, gs_texture_t *probability, std::uint32_t width, std::uint32_t height)
{
	blurredFrame_ = nullptr;
	const std::uint32_t maskWidth = gs_texture_get_width(probability);
	const std::uint32_t maskHeight = gs_texture_get_height(probability);
	ensureMaskTextures(maskWidth, maskHeight);

	// Each draw sets its parameters in the gs_effect_loop() body, right before gs_draw_sprite(); the
	// end of the loop clears them all.
	gs_effect_t *const effect = effect_.effect;
	gs_enable_blending(false);
	setTarget(refinedMask_.get(), maskWidth, maskHeight);
	while (gs_effect_loop(effect, "RefineMask")) {
		gs_effect_set_texture(effect_.epProbability, probability);
		gs_effect_set_float(effect_.epThreshold, kForegroundThreshold);
		gs_effect_set_int(effect_.epMaskWidth, static_cast<int>(maskWidth));
		gs_effect_set_int(effect_.epMaskHeight, static_cast<int>(maskHeight));
		gs_draw_sprite(nullptr, 0, maskWidth, maskHeight);
	}

	if (!blurEnabled_) {
		return;
	}
	const std::size_t levels = blurLevels(blurStrength_);
	if (levels == 0) {
		blurredFrame_ = image;
		return;
	}
	ensurePyramid(width, height, levels);

	// The focus distance, once per frame at the size of the mask.
	setTarget(focusDistance_.get(), maskWidth, maskHeight);
	while (gs_effect_loop(effect, "FocusDistance")) {
		gs_effect_set_texture(effect_.epMask, refinedMask_.get());
		gs_effect_set_float(effect_.epFocusPoint, blurFocusPoint_);
		gs_draw_sprite(nullptr, 0, maskWidth, maskHeight);
	}

	// Downsample: level k from level k - 1, halfPixel of level k.
	for (std::size_t k = 1; k <= levels; ++k) {
		const BlurLevel &level = pyramid_[k];
		gs_texture_t *const above = k == 1 ? image : pyramid_[k - 1].down.get();
		vec2 halfPixel;
		vec2_set(&halfPixel, 0.5F / static_cast<float>(level.width), 0.5F / static_cast<float>(level.height));
		setTarget(level.down.get(), level.width, level.height);
		while (gs_effect_loop(effect, "Downsample")) {
			gs_effect_set_texture(effect_.epImage, above);
			gs_effect_set_vec2(effect_.epHalfPixel, &halfPixel);
			gs_draw_sprite(nullptr, 0, level.width, level.height);
		}
	}

	// Upsample: level k - 1 from level k, halfPixel of level k, blended with level k - 1 before the
	// blur by the weight of level k.
	gs_texture_t *lower = pyramid_[levels].down.get();
	for (std::size_t k = levels; k >= 1; --k) {
		const BlurLevel &source = pyramid_[k];
		const BlurLevel &target = pyramid_[k - 1];
		gs_texture_t *const unblurred = k == 1 ? image : target.down.get();
		const auto levelOffset = static_cast<float>(blurLevelOffset(k));
		const auto levelScale = static_cast<float>(blurLevelScale(k));
		vec2 halfPixel;
		vec2_set(&halfPixel, 0.5F / static_cast<float>(source.width), 0.5F / static_cast<float>(source.height));
		setTarget(target.up.get(), target.width, target.height);
		while (gs_effect_loop(effect, "Upsample")) {
			gs_effect_set_texture(effect_.epBlurred, lower);
			gs_effect_set_texture(effect_.epImage, unblurred);
			gs_effect_set_texture(effect_.epFocus, focusDistance_.get());
			gs_effect_set_vec2(effect_.epHalfPixel, &halfPixel);
			gs_effect_set_float(effect_.epStrength, blurStrength_);
			gs_effect_set_float(effect_.epLevelOffset, levelOffset);
			gs_effect_set_float(effect_.epLevelScale, levelScale);
			gs_draw_sprite(nullptr, 0, target.width, target.height);
		}
		lower = target.up.get();
	}
	blurredFrame_ = pyramid_[0].up.get();
}

void MaskCompositor::draw(gs_texture_t *image, std::uint32_t width, std::uint32_t height) noexcept
{
	gs_effect_t *const effect = effect_.effect;
	if (blurredFrame_) {
		while (gs_effect_loop(effect, "DrawBlurred")) {
			gs_effect_set_texture(effect_.epBlurred, blurredFrame_);
			gs_draw_sprite(nullptr, 0, width, height);
		}
		return;
	}
	while (gs_effect_loop(effect, "RemoveBackground")) {
		gs_effect_set_texture(effect_.epImage, image);
		gs_effect_set_texture(effect_.epMask, refinedMask_.get());
		gs_draw_sprite(nullptr, 0, width, height);
	}
}

void MaskCompositor::ensureMaskTextures(std::uint32_t width, std::uint32_t height)
{
	if (refinedMask_ && maskTexturesWidth_ == width && maskTexturesHeight_ == height) {
		return;
	}
	refinedMask_.reset(gs_texture_create(width, height, GS_R8, 1, nullptr, GS_RENDER_TARGET));
	focusDistance_.reset(gs_texture_create(width, height, GS_R8, 1, nullptr, GS_RENDER_TARGET));
	if (!refinedMask_ || !focusDistance_) {
		throw std::runtime_error("Failed to create the MaskCompositor mask textures");
	}
	maskTexturesWidth_ = width;
	maskTexturesHeight_ = height;
}

void MaskCompositor::ensurePyramid(std::uint32_t width, std::uint32_t height, std::size_t levels)
{
	if (pyramid_.empty() || pyramid_[0].width != width || pyramid_[0].height != height) {
		pyramid_.clear();
		BlurLevel top;
		top.width = width;
		top.height = height;
		top.up.reset(gs_texture_create(width, height, GS_BGRA, 1, nullptr, GS_RENDER_TARGET));
		if (!top.up) {
			throw std::runtime_error("Failed to create the MaskCompositor blur pyramid");
		}
		pyramid_.push_back(std::move(top));
	}
	while (pyramid_.size() <= levels) {
		const BlurLevel &above = pyramid_.back();
		BlurLevel level;
		level.width = blurLevelSize(above.width);
		level.height = blurLevelSize(above.height);
		level.down.reset(gs_texture_create(level.width, level.height, GS_BGRA, 1, nullptr, GS_RENDER_TARGET));
		level.up.reset(gs_texture_create(level.width, level.height, GS_BGRA, 1, nullptr, GS_RENDER_TARGET));
		if (!level.down || !level.up) {
			throw std::runtime_error("Failed to create the MaskCompositor blur pyramid");
		}
		pyramid_.push_back(std::move(level));
	}
}

} // namespace BackgroundRemoval
