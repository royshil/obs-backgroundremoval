// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Effects/BackgroundRemovalEffect.hpp"

#include <stdexcept>

namespace Effects {
namespace {

constexpr const char *backgroundRemovalEffectSource = R"effect(
uniform float4x4 ViewProj;

uniform texture2d image;
uniform texture2d alphamask;
uniform texture2d blurredBackground;
uniform texture2d focalmask;

uniform float xOffset;
uniform float yOffset;
uniform int blurIter;
uniform int blurTotal;
uniform float blurFocusPoint;
uniform float blurFocusDepth;

sampler_state textureSampler {
	Filter = Linear;
	AddressU = Clamp;
	AddressV = Clamp;
};

struct VertDataIn {
	float4 pos : POSITION;
	float2 uv : TEXCOORD0;
};

struct VertDataOut {
	float4 pos : POSITION;
	float2 uv : TEXCOORD0;
};

VertDataOut VSDefault(VertDataIn v_in)
{
	VertDataOut vert_out;
	vert_out.pos = mul(float4(v_in.pos.xyz, 1.0), ViewProj);
	vert_out.uv = v_in.uv;
	return vert_out;
}

float4 PSTakeBlur(VertDataOut v_in) : TARGET
{
	return float4(blurredBackground.Sample(textureSampler, v_in.uv).rgb, 1.0);
}

float4 PSDraw(VertDataOut v_in) : TARGET
{
	return image.Sample(textureSampler, v_in.uv);
}

float4 PSExtractScaledRGBPlanes(VertDataOut v_in) : TARGET
{
	float packedY = v_in.uv.y * 3.0;
	int plane = min(int(packedY), 2);
	float4 pixel = image.Sample(textureSampler, float2(v_in.uv.x, frac(packedY)));
	float value = plane == 0 ? pixel.r : (plane == 1 ? pixel.g : pixel.b);
	return float4(value, 0.0, 0.0, 1.0);
}

float4 PSAlphaMaskRGBAWithoutBlur(VertDataOut v_in) : TARGET
{
	float4 inputRGBA = image.Sample(textureSampler, v_in.uv);
	inputRGBA.rgb = max(float3(0.0, 0.0, 0.0), inputRGBA.rgb / inputRGBA.a);

	float4 outputRGBA;
	float a = (1.0 - alphamask.Sample(textureSampler, v_in.uv).r) * inputRGBA.a;
	outputRGBA.rgb = inputRGBA.rgb * a;
	outputRGBA.a = a;
	return outputRGBA;
}

float4 PSKawaseFocalBlur(VertDataOut v_in) : TARGET
{
	float blurIterF = float(blurIter) / float(blurTotal);

	float blurValue = focalmask.Sample(textureSampler, v_in.uv).r;
	blurValue += focalmask.Sample(textureSampler, v_in.uv + float2(0.01, 0.01)).r;
	blurValue += focalmask.Sample(textureSampler, v_in.uv + float2(-0.01, 0.01)).r;
	blurValue += focalmask.Sample(textureSampler, v_in.uv + float2(0.01, -0.01)).r;
	blurValue += focalmask.Sample(textureSampler, v_in.uv + float2(-0.01, -0.01)).r;
	blurValue *= 0.2;

	float blurFocusDistance = clamp(abs(blurValue - blurFocusPoint), 0.0, 1.0);
	float blurFocusFactor = clamp(blurFocusDistance - blurFocusDepth, 0.0, 1.0);

	if (blurIterF > blurFocusFactor) {
		return image.Sample(textureSampler, v_in.uv);
	}

	float4 sum = float4(0.0, 0.0, 0.0, 0.0);
	sum += image.Sample(textureSampler, v_in.uv + float2(xOffset, yOffset));
	sum += image.Sample(textureSampler, v_in.uv + float2(-xOffset, yOffset));
	sum += image.Sample(textureSampler, v_in.uv + float2(xOffset, -yOffset));
	sum += image.Sample(textureSampler, v_in.uv + float2(-xOffset, -yOffset));
	sum *= 0.25;
	return sum;
}

technique DrawWithFocalBlur
{
	pass
	{
		vertex_shader = VSDefault(v_in);
		pixel_shader = PSTakeBlur(v_in);
	}
}

technique DrawWithoutBlur
{
	pass
	{
		vertex_shader = VSDefault(v_in);
		pixel_shader = PSAlphaMaskRGBAWithoutBlur(v_in);
	}
}

technique DrawFocalBlur
{
	pass
	{
		vertex_shader = VSDefault(v_in);
		pixel_shader = PSKawaseFocalBlur(v_in);
	}
}

technique Draw
{
	pass
	{
		vertex_shader = VSDefault(v_in);
		pixel_shader = PSDraw(v_in);
	}
}

technique ExtractScaledRGBPlanes
{
	pass
	{
		vertex_shader = VSDefault(v_in);
		pixel_shader = PSExtractScaledRGBPlanes(v_in);
	}
}
)effect";

class TextureRenderGuard final {
public:
	explicit TextureRenderGuard(gs_texture_t *targetTexture)
		: previousRenderTarget_(gs_get_render_target()),
		  previousZStencil_(gs_get_zstencil_target()),
		  previousColorSpace_(gs_get_color_space())
	{
		gs_set_render_target_with_color_space(targetTexture, nullptr, GS_CS_SRGB);
		gs_viewport_push();
		gs_projection_push();
		gs_matrix_push();
		gs_blend_state_push();

		const std::uint32_t width = gs_texture_get_width(targetTexture);
		const std::uint32_t height = gs_texture_get_height(targetTexture);
		gs_set_viewport(0, 0, static_cast<int>(width), static_cast<int>(height));
		gs_ortho(0.0F, static_cast<float>(width), 0.0F, static_cast<float>(height), -100.0F, 100.0F);
		gs_matrix_identity();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
	}

	~TextureRenderGuard() noexcept
	{
		gs_blend_state_pop();
		gs_matrix_pop();
		gs_projection_pop();
		gs_viewport_pop();
		gs_set_render_target_with_color_space(previousRenderTarget_, previousZStencil_, previousColorSpace_);
	}

	TextureRenderGuard(const TextureRenderGuard &) = delete;
	TextureRenderGuard(TextureRenderGuard &&) = delete;
	auto operator=(const TextureRenderGuard &) -> TextureRenderGuard & = delete;
	auto operator=(TextureRenderGuard &&) -> TextureRenderGuard & = delete;

private:
	gs_texture_t *const previousRenderTarget_;
	gs_zstencil_t *const previousZStencil_;
	const gs_color_space previousColorSpace_;
};

auto getEffectParameter(gs_effect_t *effect, const char *name) noexcept -> gs_eparam_t *
{
	if (!effect) {
		return nullptr;
	}
	return gs_effect_get_param_by_name(effect, name);
}

} // namespace

BackgroundRemovalEffect::BackgroundRemovalEffect()
	: effect_(gs_effect_create(backgroundRemovalEffectSource, "background_removal.effect", nullptr)),
	  image_(getEffectParameter(effect_, "image")),
	  alphaMask_(getEffectParameter(effect_, "alphamask")),
	  blurredBackground_(getEffectParameter(effect_, "blurredBackground")),
	  focalMask_(getEffectParameter(effect_, "focalmask")),
	  xOffset_(getEffectParameter(effect_, "xOffset")),
	  yOffset_(getEffectParameter(effect_, "yOffset")),
	  blurIteration_(getEffectParameter(effect_, "blurIter")),
	  blurTotal_(getEffectParameter(effect_, "blurTotal")),
	  blurFocusPoint_(getEffectParameter(effect_, "blurFocusPoint")),
	  blurFocusDepth_(getEffectParameter(effect_, "blurFocusDepth"))
{
	if (!effect_) {
		throw std::runtime_error("Failed to create background removal effect");
	}

	if (!image_ || !alphaMask_ || !blurredBackground_ || !focalMask_ || !xOffset_ || !yOffset_ || !blurIteration_ ||
	    !blurTotal_ || !blurFocusPoint_ || !blurFocusDepth_) {
		gs_effect_destroy(effect_);
		throw std::runtime_error("Background removal effect parameter was not found");
	}
}

BackgroundRemovalEffect::~BackgroundRemovalEffect() noexcept
{
	gs_effect_destroy(effect_);
}

void BackgroundRemovalEffect::renderSource(gs_texture_t *targetTexture, obs_source_t *source) const noexcept
{
	TextureRenderGuard renderGuard(targetTexture);
	vec4 clearColor;
	vec4_zero(&clearColor);
	gs_clear(GS_CLEAR_COLOR, &clearColor, 0.0F, 0);
	while (gs_effect_loop(effect_, "Draw")) {
		obs_source_video_render(source);
	}
}

void BackgroundRemovalEffect::extractScaledRGBPlanes(gs_texture_t *targetTexture,
						     gs_texture_t *sourceTexture) const noexcept
{
	TextureRenderGuard renderGuard(targetTexture);
	gs_effect_set_texture(image_, sourceTexture);
	while (gs_effect_loop(effect_, "ExtractScaledRGBPlanes")) {
		gs_draw_sprite(sourceTexture, 0, gs_texture_get_width(targetTexture),
			       gs_texture_get_height(targetTexture));
	}
}

void BackgroundRemovalEffect::focalBlur(gs_texture_t *targetTexture, gs_texture_t *sourceTexture,
					gs_texture_t *focalMaskTexture, std::int64_t iteration, std::int64_t total,
					float focusPoint, float focusDepth) const noexcept
{
	TextureRenderGuard renderGuard(targetTexture);
	gs_effect_set_texture(image_, sourceTexture);
	gs_effect_set_texture(focalMask_, focalMaskTexture);
	gs_effect_set_float(xOffset_, (static_cast<float>(iteration) + 0.5F) /
					      static_cast<float>(gs_texture_get_width(sourceTexture)));
	gs_effect_set_float(yOffset_, (static_cast<float>(iteration) + 0.5F) /
					      static_cast<float>(gs_texture_get_height(sourceTexture)));
	gs_effect_set_int(blurIteration_, static_cast<int>(iteration));
	gs_effect_set_int(blurTotal_, static_cast<int>(total));
	gs_effect_set_float(blurFocusPoint_, focusPoint);
	gs_effect_set_float(blurFocusDepth_, focusDepth);
	while (gs_effect_loop(effect_, "DrawFocalBlur")) {
		gs_draw_sprite(sourceTexture, 0, 0, 0);
	}
}

void BackgroundRemovalEffect::directDrawWithBlurredBackground(gs_texture_t *sourceTexture,
							      gs_texture_t *alphaMaskTexture,
							      gs_texture_t *blurredBackgroundTexture) const noexcept
{
	while (gs_effect_loop(effect_, "DrawWithFocalBlur")) {
		gs_effect_set_texture(image_, sourceTexture);
		gs_effect_set_texture(alphaMask_, alphaMaskTexture);
		gs_effect_set_texture(blurredBackground_, blurredBackgroundTexture);
		gs_draw_sprite(sourceTexture, 0, 0, 0);
	}
}

void BackgroundRemovalEffect::directDrawWithoutBlur(gs_texture_t *sourceTexture,
						    gs_texture_t *alphaMaskTexture) const noexcept
{
	while (gs_effect_loop(effect_, "DrawWithoutBlur")) {
		gs_effect_set_texture(image_, sourceTexture);
		gs_effect_set_texture(alphaMask_, alphaMaskTexture);
		gs_draw_sprite(sourceTexture, 0, 0, 0);
	}
}

} // namespace Effects
