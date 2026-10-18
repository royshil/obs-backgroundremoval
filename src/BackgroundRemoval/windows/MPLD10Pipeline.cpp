// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include "MPLD10Pipeline.hpp"

#include <TraceLoggingProvider.h>
#include <TraceLoggingActivity.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <source_location>

#include <graphics/graphics.h>
#include <util/platform.h>

#include <BackgroundRemoval/windows/MediaPipeLandscapeDirectML10/MPLD10Engine.hpp>

TRACELOGGING_DECLARE_PROVIDER(g_hProvider);

namespace BackgroundRemoval::MPLD10Detail {

const char *gateEffectString = R"EFFECT(
uniform float4x4 ViewProj;
uniform texture2d image;
uniform texture2d image1;

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

float luma709(float3 color)
{
	return dot(color, float3(0.2126, 0.7152, 0.0722));
}

float4 PSDraw(VertDataOut v_in) : TARGET
{
	return image.Sample(textureSampler, v_in.uv);
}

float4 PSLumaAbsDiff(VertDataOut v_in) : TARGET
{
	float4 leftPixel = image.Sample(textureSampler, v_in.uv);
	float4 rightPixel = image1.Sample(textureSampler, v_in.uv);
	float value = abs(luma709(leftPixel.rgb) - luma709(rightPixel.rgb));
	return float4(value, value, value, 1.0);
}

technique LumaAbsDiff
{
	pass
	{
		vertex_shader = VSDefault(v_in);
		pixel_shader = PSLumaAbsDiff(v_in);
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
)EFFECT";

const char *renderingEffectString = R"EFFECT(
uniform float4x4 ViewProj;

uniform texture2d image;
uniform texture2d image1;
uniform texture2d image2;


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
	return float4(image2.Sample(textureSampler, v_in.uv).rgb, 1.0);
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

float sampleMask(float2 uv)
{
	return image1.Sample(textureSampler, uv).r > 0.5 ? 1.0 : 0.0;
}

float4 PSAlphaMaskRGBAWithoutBlur(VertDataOut v_in) : TARGET
{
	float4 inputRGBA = image.Sample(textureSampler, v_in.uv);
	inputRGBA.rgb = max(float3(0.0, 0.0, 0.0), inputRGBA.rgb / inputRGBA.a);

	float4 outputRGBA;
	float a = (1.0 - sampleMask(v_in.uv)) * inputRGBA.a;
	outputRGBA.rgb = inputRGBA.rgb * a;
	outputRGBA.a = a;
	return outputRGBA;
}

float4 PSKawaseFocalBlur(VertDataOut v_in) : TARGET
{
	float blurIterF = float(blurIter) / float(blurTotal);

	float blurValue = sampleMask(v_in.uv);
	blurValue += sampleMask(v_in.uv + float2(0.01, 0.01));
	blurValue += sampleMask(v_in.uv + float2(-0.01, 0.01));
	blurValue += sampleMask(v_in.uv + float2(0.01, -0.01));
	blurValue += sampleMask(v_in.uv + float2(-0.01, -0.01));
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
)EFFECT";

} // namespace BackgroundRemoval::MPLD10Detail

namespace {

using InitError = BackgroundRemoval::MPLD10Pipeline::InitError;

auto createEngine() -> std::unique_ptr<BackgroundRemoval::MPLD10Engine>
{
	if (gs_get_device_type() != GS_DEVICE_DIRECT3D_11)
		throw std::invalid_argument("MPLD10Pipeline requires the D3D11 backend");
	return std::make_unique<BackgroundRemoval::MPLD10Engine>(static_cast<ID3D11Device *>(gs_get_device_obj()),
								 BackgroundRemoval::MPLD10Engine::Output::GpuMask);
}

auto gs_effect_create_unique(const char *effect_string, const char *filename,
			     std::source_location loc = std::source_location::current())
	-> std::unique_ptr<gs_effect_t, void (*)(gs_effect_t *)>
{
	gs_effect_t *value = gs_effect_create(effect_string, filename, nullptr);
	if (!value) {
		throw InitError(loc);
	}
	return {value, gs_effect_destroy};
}

auto gs_texture_create_unique(std::uint32_t width, std::uint32_t height, gs_color_format color_format, uint32_t levels,
			      const uint8_t **data, uint32_t flags,
			      std::source_location loc = std::source_location::current())
	-> std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)>
{
	gs_texture_t *value = gs_texture_create(width, height, color_format, levels, data, flags);
	if (!value) {
		throw InitError(loc);
	}
	return {value, gs_texture_destroy};
}

auto gs_stagesurface_create_unique(std::uint32_t width, std::uint32_t height, gs_color_format format,
				   std::source_location loc = std::source_location::current())
	-> std::unique_ptr<gs_stagesurf_t, void (*)(gs_stagesurf_t *)>
{
	gs_stagesurf_t *value = gs_stagesurface_create(width, height, format);
	if (!value)
		throw InitError(loc);
	return {value, gs_stagesurface_destroy};
}

struct RenderStatePop {
	gs_texture_t *rt_ = gs_get_render_target();
	gs_zstencil_t *zs_ = gs_get_zstencil_target();
	gs_color_space cs_ = gs_get_color_space();
	bool active_ = true;
	RenderStatePop()
	{
		gs_blend_state_push();
		gs_matrix_push();
		gs_projection_push();
		gs_viewport_push();
	}
	~RenderStatePop() { (*this)(); }
	void operator()()
	{
		if (active_) {
			active_ = false;
			gs_blend_state_pop();
			gs_matrix_pop();
			gs_projection_pop();
			gs_viewport_pop();
			gs_set_render_target_with_color_space(rt_, zs_, cs_);
		}
	}
};

} // namespace

namespace BackgroundRemoval {

MPLD10Pipeline::MPLD10Pipeline(const FilterProperty &property, obs_source_t *source, std::uint32_t width,
			       std::uint32_t height)
	: engine_{createEngine()},
	  width_(width),
	  height_(height),
	  source_(source),
	  renderingEffect_{gs_effect_create_unique(MPLD10Detail::renderingEffectString, "MPLD10Rendering.effect")},
	  epImage_{gs_effect_get_param_by_name(renderingEffect_.get(), "image")},
	  epImage1_{gs_effect_get_param_by_name(renderingEffect_.get(), "image1")},
	  epImage2_{gs_effect_get_param_by_name(renderingEffect_.get(), "image2")},
	  gateEffect_{gs_effect_create_unique(MPLD10Detail::gateEffectString, "MPLD10Gate.effect")},
	  gateImage_{gs_effect_get_param_by_name(gateEffect_.get(), "image")},
	  gateImage1_{gs_effect_get_param_by_name(gateEffect_.get(), "image1")},
	  targetTexture_{gs_texture_create_unique(width, height, GS_BGRA, 1, nullptr, GS_RENDER_TARGET)},
	  inputTexture_{gs_texture_create_unique(kModelWidth, kModelHeight, GS_BGRA, 1, nullptr, GS_RENDER_TARGET)},
	  lastInputTexture_{gs_texture_create_unique(kModelWidth, kModelHeight, GS_BGRA, 1, nullptr, GS_RENDER_TARGET)},
	  lumaAbsDiffTexture_{gs_texture_create_unique(kModelWidth, kModelHeight, GS_R8, 1, nullptr, GS_RENDER_TARGET)},
	  lumaAbsDiffStagesurf_{gs_stagesurface_create_unique(kModelWidth, kModelHeight, GS_R8)},
	  blurTextures_{gs_texture_create_unique(width, height, GS_BGRA, 1, nullptr, GS_RENDER_TARGET),
			gs_texture_create_unique(width, height, GS_BGRA, 1, nullptr, GS_RENDER_TARGET)},
	  alphaTexture_{gs_texture_wrap_obj(engine_->maskTexture()), gs_texture_destroy}
{
	if (!source || width == 0 || height == 0 || !alphaTexture_) {
		throw std::invalid_argument("Invalid MediaPipe Landscape DirectML pipeline arguments");
	}
	obs_enter_graphics();
	auto *previousTarget = gs_get_render_target();
	auto *previousDepth = gs_get_zstencil_target();
	auto previousSpace = gs_get_color_space();
	vec4 black{};
	gs_set_render_target_with_color_space(lastInputTexture_.get(), nullptr, GS_CS_709_EXTENDED);
	gs_clear(GS_CLEAR_COLOR, &black, 0, 0);
	gs_set_render_target_with_color_space(inputTexture_.get(), nullptr, GS_CS_709_EXTENDED);
	gs_clear(GS_CLEAR_COLOR, &black, 0, 0);

	gs_set_render_target_with_color_space(previousTarget, previousDepth, previousSpace);

	obs_leave_graphics();
	update(property);
}

MPLD10Pipeline::~MPLD10Pipeline() noexcept
{
	obs_enter_graphics();
	renderingEffect_.reset();
	gateEffect_.reset();
	lastInputTexture_.reset();
	inputTexture_.reset();
	lumaAbsDiffTexture_.reset();
	lumaAbsDiffStagesurf_.reset();
	alphaTexture_.reset();
	engine_.reset();
	for (auto &texture : blurTextures_)
		texture.reset();
	targetTexture_.reset();
	obs_leave_graphics();
}

auto MPLD10Pipeline::autoSetSensitivity() noexcept -> bool
{
	const auto sensitivity = differenceHistory_.sensitivity(os_gettime_ns());
	if (!sensitivity)
		return false;
	obs_data_t *settings = obs_data_create();
	obs_data_set_double(settings, "v2_Model_MatteSensitivity", *sensitivity);
	obs_source_update(source_, settings);
	obs_data_release(settings);
	return true;
}

void MPLD10Pipeline::update(const FilterProperty &property) noexcept
{
	blurBackgroundEnabled_.store(property.blurBackgroundEnabled, std::memory_order_relaxed);
	blurBackgroundFactor_.store(property.blurBackgroundFactor, std::memory_order_relaxed);
	blurFocusPoint_.store(static_cast<float>(property.blurFocusPoint), std::memory_order_relaxed);
	blurFocusDepth_.store(static_cast<float>(property.blurFocusDepth), std::memory_order_relaxed);
	imageSimilarityThreshold_.store(static_cast<float>(property.imageSimilarityThreshold),
					std::memory_order_relaxed);
	refineMaskThreshold_.store(static_cast<float>(property.refineMaskThreshold), std::memory_order_relaxed);
}

void MPLD10Pipeline::videoTick(float) noexcept
{
	if (differenceStaged_) {
		TraceLoggingThreadActivity<g_hProvider, 0x1> trace;
		TraceLoggingWriteStart(trace, "MPLD10Pipeline.ReadsDifference");
		bool valid = false;
		std::uint64_t sum = 0;
		static_assert(std::uint64_t(kModelWidth) * kModelHeight <=
			      std::numeric_limits<decltype(sum)>::max() / 255);
		std::uint8_t *data = nullptr;
		std::uint32_t linesize = 0;
		obs_enter_graphics();
		if (!lumaAbsDiffStagesurf_ || !gs_stagesurface_map(lumaAbsDiffStagesurf_.get(), &data, &linesize)) {
			obs_leave_graphics();
			// Retain the submitted frame and retry Map next tick before reusing settings.
			blog(LOG_ERROR, OBS_LOG_HEADER "MPLD10Pipeline.MapDifferenceError");
			return;
		}
		if (data && linesize >= kModelWidth) {
			for (std::uint32_t y = 0; y < kModelHeight; ++y) {
				const auto *row = data + std::size_t(y) * linesize;
				for (std::uint32_t x = 0; x < kModelWidth; ++x)
					sum += row[x];
			}
			valid = true;
		}
		gs_stagesurface_unmap(lumaAbsDiffStagesurf_.get());
		obs_leave_graphics();
		differenceStaged_ = false;
		if (valid) {
			differenceHistory_.record(differenceTimestamp_, double(sum) / kModelInputMaximumSum);
			const double sensitivity = imageSimilarityThreshold_.load(std::memory_order_relaxed);
			bool motion = !std::isfinite(sensitivity);
			if (!motion) {
				const auto threshold = static_cast<std::uint64_t>(std::clamp(
					std::ceil(kModelInputMaximumSum * std::pow(10.0, -sensitivity / 20.0)), 0.0,
					kModelInputMaximumSum + 1));
				motion = sum != 0 && sum >= threshold;
			}
			if (motion)
				remainingInferenceFrames_ =
					1 + additionalInferenceFrames_.load(std::memory_order_relaxed);
		} else {
			blog(LOG_ERROR, OBS_LOG_HEADER "MPLD10Pipeline.ReadDifferenceError");
			remainingInferenceFrames_ = std::max(remainingInferenceFrames_, 1u);
		}
	}
	filterState_ = FilterState::NeedsProcessing;
}

auto MPLD10Pipeline::needsRecreation() const noexcept -> bool
{
	return needsRecreation_;
}

void MPLD10Pipeline::videoRender(gs_effect_t *) noexcept
{
	vec4 zero;
	vec4_zero(&zero);

	gs_effect_t *const effect = renderingEffect_.get();
	gs_eparam_t *const epImage = epImage_;
	gs_eparam_t *const epImage1 = epImage1_;
	gs_eparam_t *const epImage2 = epImage2_;

	assert(effect);
	assert(epImage);
	assert(epImage1);
	assert(epImage2);

	gs_texture_t *const targetTexture = targetTexture_.get();
	gs_texture_t *const inputTexture = inputTexture_.get();
	gs_texture_t *const alphaTexture = alphaTexture_.get();

	TraceLoggingFunction(g_hProvider, TraceLoggingKeyword(0x1));

	obs_source_t *const source = source_;
	if (!source || !obs_source_enabled(source)) {
		obs_source_skip_video_filter(source);
		return;
	}

	obs_source_t *const target = obs_filter_get_target(source);
	if (!target) {
		obs_source_skip_video_filter(source);
		return;
	}

	const std::uint32_t sourceWidth = obs_source_get_width(target);
	const std::uint32_t sourceHeight = obs_source_get_height(target);
	if (sourceWidth == 0 || sourceHeight == 0) {
		obs_source_skip_video_filter(source);
		return;
	}
	const float sourceWidthF = static_cast<float>(sourceWidth);
	const float sourceHeightF = static_cast<float>(sourceHeight);

	const double imageSimilarityThreshold = imageSimilarityThreshold_.load(std::memory_order_relaxed);
	const bool blurBackgroundEnabled = blurBackgroundEnabled_.load(std::memory_order_relaxed);
	const auto blurIterations =
		std::clamp<std::int64_t>(blurBackgroundFactor_.load(std::memory_order_relaxed), 0, 30);
	const float blurFocusPoint = blurFocusPoint_.load(std::memory_order_relaxed);
	const float blurFocusDepth = blurFocusDepth_.load(std::memory_order_relaxed);
	const float refineMaskThreshold = refineMaskThreshold_.load(std::memory_order_relaxed);

	const bool willGenerateMask = filterState_ == FilterState::NeedsProcessing && !needsRecreation_;

	const bool willInferMask = willGenerateMask && remainingInferenceFrames_ > 0;
	bool willRecreateEngine = false;
	bool willBlurBackground = filterState_ == FilterState::NeedsProcessing && blurBackgroundEnabled &&
				  blurIterations > 0;

	if (filterState_ == FilterState::Processing) {
		return;
	}

	if (filterState_ == FilterState::NeedsProcessing) {
		filterState_ = FilterState::Processing;
	}

	RenderStatePop renderStatePop;
	gs_matrix_identity();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

	gs_set_render_target_with_color_space(targetTexture, nullptr, GS_CS_709_EXTENDED);
	gs_ortho(0, sourceWidthF, 0, sourceHeightF, -100, 100);
	gs_set_viewport(0, 0, width_, height_);
	gs_clear(GS_CLEAR_COLOR, &zero, 0, 0);
	while (gs_effect_loop(effect, "Draw")) {
		obs_source_video_render(target);
	}

	if (willGenerateMask && inputTexture) {
		TraceLoggingThreadActivity<g_hProvider, 0x1> trace;
		TraceLoggingWriteStart(trace, "MPLD10Pipeline.RendersInputTexture");
		gs_set_render_target_with_color_space(inputTexture, nullptr, GS_CS_709_EXTENDED);
		gs_ortho(0, kModelWidth, 0, kModelHeight, -100, 100);
		gs_set_viewport(0, 0, kModelWidth, kModelHeight);
		gs_clear(GS_CLEAR_COLOR, &zero, 0, 0);
		while (gs_effect_loop(gateEffect_.get(), "Draw")) {
			gs_effect_set_texture(gateImage_, targetTexture);
			gs_draw_sprite(nullptr, 0, kModelWidth, kModelHeight);
		}
	}

	if (willInferMask) {
		bool skip = false;
		if (!engine_ || !alphaTexture || !inputTexture) {
			blog(LOG_ERROR, OBS_LOG_HEADER "MPLD10Pipeline.InfersMaskNullError");
			skip = true;
		}
		if (!skip) {
			if (HRESULT hr =
				    engine_->runAsync(static_cast<ID3D11Texture2D *>(gs_texture_get_obj(inputTexture)),
						      std::clamp(refineMaskThreshold, 0.0F, 1.0F));
			    FAILED(hr)) {
				blog(LOG_ERROR, OBS_LOG_HEADER "MPLD10Pipeline.RunsInferenceError: %ld", hr);
				willRecreateEngine = engine_->needsRecreation();
				skip = true;
			}
		}
		if (!skip) {
			if (HRESULT hr = engine_->ensureOutput(); FAILED(hr)) {
				blog(LOG_ERROR, OBS_LOG_HEADER "MPLD10Pipeline.EnsureOutputError: %ld", hr);
				skip = true;
			}
		}
		if (!skip) {
			--remainingInferenceFrames_;
		}
	}
	if (willRecreateEngine) {
		needsRecreation_ = true;
	}
	TraceLoggingWrite(g_hProvider, "InferenceDecision", TraceLoggingLevel(5), TraceLoggingKeyword(0x1),
			  TraceLoggingFloat64(imageSimilarityThreshold, "EffectiveSensitivity"),
			  TraceLoggingBool(willInferMask, "Required"));

	gs_texture_t *blurredBackgroundTexture = targetTexture;
	for (std::int64_t iteration = 0;
	     willBlurBackground && targetTexture && alphaTexture && iteration < blurIterations; ++iteration) {
		gs_ortho(0, static_cast<float>(width_), 0, static_cast<float>(height_), -100, 100);
		gs_set_viewport(0, 0, width_, height_);
		auto *destination = blurTextures_[iteration % blurTextures_.size()].get();
		if (!destination) {
			blog(LOG_ERROR, OBS_LOG_HEADER "MPLD10Pipeline.BlursBackgroundNullError: destination");
			return;
		}
		gs_set_render_target_with_color_space(destination, nullptr, GS_CS_709_EXTENDED);
		while (gs_effect_loop(effect, "DrawFocalBlur")) {
			gs_effect_set_texture(epImage, blurredBackgroundTexture);
			gs_effect_set_texture(epImage1, alphaTexture);
			gs_effect_set_float(gs_effect_get_param_by_name(effect, "xOffset"),
					    (static_cast<float>(iteration) + .5F) / width_);
			gs_effect_set_float(gs_effect_get_param_by_name(effect, "yOffset"),
					    (static_cast<float>(iteration) + .5F) / height_);
			gs_effect_set_int(gs_effect_get_param_by_name(effect, "blurIter"), static_cast<int>(iteration));
			gs_effect_set_int(gs_effect_get_param_by_name(effect, "blurTotal"),
					  static_cast<int>(blurIterations));
			gs_effect_set_float(gs_effect_get_param_by_name(effect, "blurFocusPoint"), blurFocusPoint);
			gs_effect_set_float(gs_effect_get_param_by_name(effect, "blurFocusDepth"), blurFocusDepth);
			gs_draw_sprite(nullptr, 0, width_, height_);
		}
		blurredBackgroundTexture = destination;
	}

	if (filterState_ == FilterState::Processing) {
		frameBlurBackgroundEnabled_ = blurBackgroundEnabled;
		frameBlurredTexture_ = blurredBackgroundTexture;
		filterState_ = FilterState::ReadyForRendering;
	}

	renderStatePop();
	if (filterState_ == FilterState::ReadyForRendering) {
		const char *technique = frameBlurBackgroundEnabled_ && frameBlurredTexture_ ? "DrawWithFocalBlur"
											    : "DrawWithoutBlur";
		while (gs_effect_loop(effect, technique)) {
			gs_effect_set_texture(epImage, targetTexture);
			gs_effect_set_texture(epImage1, alphaTexture);
			gs_effect_set_texture(epImage2, frameBlurredTexture_);
			gs_draw_sprite(nullptr, 0, width_, height_);
		}
	}

	if (willGenerateMask && inputTexture && lastInputTexture_ && lumaAbsDiffTexture_ && lumaAbsDiffStagesurf_) {
		RenderStatePop differenceStatePop;
		gs_matrix_identity();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
		TraceLoggingThreadActivity<g_hProvider, 0x1> trace;
		TraceLoggingWriteStart(trace, "MPLD10Pipeline.StagesDifference");
		gs_set_render_target_with_color_space(lumaAbsDiffTexture_.get(), nullptr, GS_CS_709_EXTENDED);
		gs_ortho(0, kModelWidth, 0, kModelHeight, -100, 100);
		gs_set_viewport(0, 0, kModelWidth, kModelHeight);
		gs_clear(GS_CLEAR_COLOR, &zero, 0, 0);
		while (gs_effect_loop(gateEffect_.get(), "LumaAbsDiff")) {
			gs_effect_set_texture(gateImage_, lastInputTexture_.get());
			gs_effect_set_texture(gateImage1_, inputTexture);
			gs_draw_sprite(nullptr, 0, kModelWidth, kModelHeight);
		}
		gs_stage_texture(lumaAbsDiffStagesurf_.get(), lumaAbsDiffTexture_.get());
		differenceTimestamp_ = os_gettime_ns();
		differenceStaged_ = true;
		std::swap(lastInputTexture_, inputTexture_);
	}
}

} // namespace BackgroundRemoval
