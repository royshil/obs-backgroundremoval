// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TraceLoggingProvider.hpp"
#include "MediaPipeLandscapeDirectMLPipeline.hpp"
#include "MediaPipeLandscapeDirectMLEngine.hpp"
#include "graphics/graphics.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

auto gs_effect_create_unique(const char *effect_string, const char *filename, const char *message)
	-> std::unique_ptr<gs_effect_t, void (*)(gs_effect_t *)>
{
	gs_effect_t *value = gs_effect_create(effect_string, filename, nullptr);
	if (!value) {
		throw std::runtime_error(message);
	}
	return {value, gs_effect_destroy};
}

auto gs_texture_create_unique(std::uint32_t width, std::uint32_t height, gs_color_format color_format, uint32_t levels,
			      const uint8_t **data, uint32_t flags, const char *message)
	-> std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)>
{
	gs_texture_t *value = gs_texture_create(width, height, color_format, levels, data, flags);
	if (!value) {
		throw std::runtime_error(message);
	}
	return {value, gs_texture_destroy};
}

auto gs_stagesurface_create_unique(uint32_t width, uint32_t height, gs_color_format color_format, const char *message)
	-> std::unique_ptr<gs_stagesurf_t, void (*)(gs_stagesurf_t *)>
{
	gs_stagesurf_t *value = gs_stagesurface_create(width, height, color_format);
	if (!value) {
		throw std::runtime_error(message);
	}
	return {value, gs_stagesurface_destroy};
}

} // namespace

namespace BackgroundRemoval::Detail {

const char *mpldEffectString = R"EFFECT(
uniform float4x4 ViewProj;

uniform texture2d image;
uniform texture2d image1;
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

float luma709(float3 color)
{
	return dot(color, float3(0.2126, 0.7152, 0.0722));
}

float4 PSTakeBlur(VertDataOut v_in) : TARGET
{
	return float4(blurredBackground.Sample(textureSampler, v_in.uv).rgb, 1.0);
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

technique LumaAbsDiff
{
	pass
	{
		vertex_shader = VSDefault(v_in);
		pixel_shader = PSLumaAbsDiff(v_in);
	}
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

} // namespace BackgroundRemoval::Detail

namespace BackgroundRemoval {

MediaPipeLandscapeDirectMLPipeline::MediaPipeLandscapeDirectMLPipeline(const FilterProperty &property,
								       obs_source_t *source, std::uint32_t width,
								       std::uint32_t height)
	: engine_{std::make_unique<MediaPipeLandscapeDirectMLEngine>(true)},
	  maskRefiner_{std::make_unique<MaskRefinerDirect3D>(engine_->device(), engine_->commandQueue(),
							     static_cast<ID3D11Device *>(gs_get_device_obj()),
							     kModelWidth, kModelHeight, width, height)},
	  width_(width),
	  height_(height),
	  source_(source),
	  effect_{gs_effect_create_unique(Detail::mpldEffectString, "MediaPipeLandscape.effect",
					  "Failed to create Landscape effect")},
	  sourceTexture_{gs_texture_create_unique(width, height, GS_BGRA, 1, nullptr, GS_RENDER_TARGET,
						  "Failed to create Landscape texture")},
	  lastInputTexture_{gs_texture_create_unique(kModelWidth, kModelHeight, GS_BGRA, 1, nullptr, GS_RENDER_TARGET,
						     "Failed to create reference image")},
	  inputTexture_{gs_texture_create_unique(kModelWidth, kModelHeight, GS_BGRA, 1, nullptr, GS_RENDER_TARGET,
						 "Failed to create current image")},
	  lumaAbsDiffTexture_{gs_texture_create_unique(kModelWidth, kModelHeight, GS_R8, 1, nullptr, GS_RENDER_TARGET,
						       "Failed to create difference texture")},
	  lumaAbsDiffStagesurf_{gs_stagesurface_create_unique(kModelWidth, kModelHeight, GS_R8,
							      "Failed to create difference staging surface")},
	  blurTextures_{gs_texture_create_unique(width, height, GS_BGRA, 1, nullptr, GS_RENDER_TARGET,
						 "Failed to create Landscape texture"),
			gs_texture_create_unique(width, height, GS_BGRA, 1, nullptr, GS_RENDER_TARGET,
						 "Failed to create Landscape texture")},
	  alphaTexture_{gs_texture_wrap_obj(maskRefiner_->outputTexture()), gs_texture_destroy}
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
	try {
		maskRefiner_->runAsync(nullptr, {});
		maskRefiner_->blockCpuUntilComplete();
		maskRefiner_->complete();
	} catch (...) {
		obs_leave_graphics();
		throw;
	}
	obs_leave_graphics();
	update(property);
}

MediaPipeLandscapeDirectMLPipeline::~MediaPipeLandscapeDirectMLPipeline() noexcept
{
	obs_enter_graphics();
	effect_.reset();
	lastInputTexture_.reset();
	inputTexture_.reset();
	lumaAbsDiffTexture_.reset();
	lumaAbsDiffStagesurf_.reset();
	alphaTexture_.reset();
	maskRefiner_.reset();
	engine_.reset();
	for (auto &texture : blurTextures_)
		texture.reset();
	sourceTexture_.reset();
	obs_leave_graphics();
}

void MediaPipeLandscapeDirectMLPipeline::update(const FilterProperty &property) noexcept
{
	blurBackgroundEnabled_.store(property.blurBackgroundEnabled, std::memory_order_relaxed);
	blurBackgroundFactor_.store(property.blurBackgroundFactor, std::memory_order_relaxed);
	blurFocusPoint_.store(static_cast<float>(property.blurFocusPoint), std::memory_order_relaxed);
	blurFocusDepth_.store(static_cast<float>(property.blurFocusDepth), std::memory_order_relaxed);
	imageSimilarityThreshold_.store(static_cast<float>(property.imageSimilarityThreshold),
					std::memory_order_relaxed);
	refineMaskEnabled_.store(property.refineMaskEnabled, std::memory_order_relaxed);
	refineMaskThreshold_.store(static_cast<float>(property.refineMaskThreshold), std::memory_order_relaxed);
	refineMaskExpansion_.store(property.refineMaskExpansion, std::memory_order_relaxed);
}

void MediaPipeLandscapeDirectMLPipeline::videoTick(float) noexcept
{
	filterState_ = FilterState::NeedsProcessing;
}

void MediaPipeLandscapeDirectMLPipeline::videoRender(gs_effect_t *) noexcept
{
	TraceLoggingFunction(provider, TraceLoggingKeyword(0x1));

	if (const bool filterEnabled = obs_source_enabled(source_); !filterEnabled) {
		obs_source_skip_video_filter(source_);
		return;
	}

	obs_source_t *const target = obs_filter_get_target(source_);
	if (!target) {
		obs_source_skip_video_filter(source_);
		return;
	}

	switch (filterState_) {
	case FilterState::ReadyForRendering:
		renderFrameResult();
		return;
	case FilterState::Processing:
		return;
	case FilterState::NeedsProcessing:
		break;
	}

	const double imageSimilarityThreshold = imageSimilarityThreshold_.load(std::memory_order_relaxed);

	const std::uint32_t sourceWidth = obs_source_get_width(target);
	const std::uint32_t sourceHeight = obs_source_get_height(target);
	if (sourceWidth == 0 || sourceHeight == 0) {
		obs_source_skip_video_filter(source_);
		return;
	}

	filterState_ = FilterState::Processing;

	struct RenderStatePop {
		gs_texture_t *rt_;
		gs_zstencil_t *zs_;
		gs_color_space cs_;
		bool active_ = true;
		~RenderStatePop() { (*this)(); }
		void operator()()
		{
			if (!active_)
				return;
			active_ = false;
			gs_blend_state_pop();
			gs_matrix_pop();
			gs_projection_pop();
			gs_viewport_pop();
			gs_set_render_target_with_color_space(rt_, zs_, cs_);
		}
	} renderStatePop{gs_get_render_target(), gs_get_zstencil_target(), gs_get_color_space()};

	gs_viewport_push();
	gs_projection_push();
	gs_matrix_push();
	gs_blend_state_push();

	gs_matrix_identity();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

	// MARK: - Inference Gating

	gs_effect_t *effect = effect_.get();
	gs_eparam_t *epImage = gs_effect_get_param_by_name(effect, "image");
	gs_eparam_t *epImage1 = gs_effect_get_param_by_name(effect, "image1");

	vec4 zero;
	vec4_zero(&zero);

	const bool blurBackgroundEnabled = blurBackgroundEnabled_.load(std::memory_order_relaxed);
	const std::int64_t unsafeBlurBackgroundFactor = blurBackgroundFactor_.load(std::memory_order_relaxed);
	const float blurFocusPoint = blurFocusPoint_.load(std::memory_order_relaxed);
	const float blurFocusDepth = blurFocusDepth_.load(std::memory_order_relaxed);
	const bool refineMaskEnabled = refineMaskEnabled_.load(std::memory_order_relaxed);
	const float refineMaskThreshold = refineMaskThreshold_.load(std::memory_order_relaxed);
	const std::int64_t unsafeRefineMaskExpansion = refineMaskExpansion_.load(std::memory_order_relaxed);

	gs_texture_t *const sourceTexture = sourceTexture_.get();
	const std::array<gs_texture_t *, 2> blurTextures{blurTextures_[0].get(), blurTextures_[1].get()};

	gs_set_render_target_with_color_space(inputTexture_.get(), nullptr, GS_CS_709_EXTENDED);
	gs_ortho(0.0F, static_cast<float>(sourceWidth), 0.0F, static_cast<float>(sourceHeight), -100.0F, 100.0F);
	gs_set_viewport(0, 0, static_cast<int>(gs_texture_get_width(inputTexture_.get())),
			static_cast<int>(gs_texture_get_height(inputTexture_.get())));
	gs_clear(GS_CLEAR_COLOR, &zero, 0.0F, 0);
	while (gs_effect_loop(effect, "Draw")) {
		obs_source_video_render(target);
	}

	gs_set_render_target_with_color_space(lumaAbsDiffTexture_.get(), nullptr, GS_CS_709_EXTENDED);
	gs_ortho(0.0F, kModelWidth, 0.0F, kModelHeight, -100.0F, 100.0F);
	gs_set_viewport(0, 0, kModelWidth, kModelHeight);
	gs_clear(GS_CLEAR_COLOR, &zero, 0.0F, 0);
	while (gs_effect_loop(effect, "LumaAbsDiff")) {
		gs_effect_set_texture(epImage, lastInputTexture_.get());
		gs_effect_set_texture(epImage1, inputTexture_.get());
		gs_draw_sprite(lumaAbsDiffTexture_.get(), 0, 0, 0);
	}

	gs_stage_texture(lumaAbsDiffStagesurf_.get(), lumaAbsDiffTexture_.get());
	bool skipMaskInference = false;
	std::uint8_t *data = nullptr;
	std::uint32_t linesize = 0;
	if (gs_stagesurface_map(lumaAbsDiffStagesurf_.get(), &data, &linesize)) {
		if (data && linesize >= kModelWidth) {
			std::uint64_t sum = 0;
			static_assert(std::numeric_limits<decltype(sum)>::max() >=
				      std::uint64_t{kModelWidth} * std::uint64_t{kModelHeight} * 255);

			for (std::uint32_t y = 0; y < kModelHeight; ++y) {
				const std::uint8_t *row = data + std::size_t(y) * linesize;
				for (std::uint32_t x = 0; x < kModelWidth; ++x) {
					sum += row[x];
				}
			}

			const auto sumThreshold = static_cast<std::uint64_t>(
				std::ceil(255.0 * kModelPixelCount * std::pow(10.0, -imageSimilarityThreshold / 20.0)));

			skipMaskInference = sum == 0 || sum < sumThreshold;
		}

		gs_stagesurface_unmap(lumaAbsDiffStagesurf_.get());
	}

	// MARK: - Running Inference

	if (!skipMaskInference) {
		try {
			if (engine_->runAsync(inputTexture_.get()) ==
			    MediaPipeLandscapeDirectMLEngine::RunResult::NeedsRecreation) {
				auto engine = std::make_unique<MediaPipeLandscapeDirectMLEngine>(true);
				auto refiner = std::make_unique<MaskRefinerDirect3D>(
					engine->device(), engine->commandQueue(),
					static_cast<ID3D11Device *>(gs_get_device_obj()), kModelWidth, kModelHeight,
					width_, height_);
				std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> alpha{
					gs_texture_wrap_obj(refiner->outputTexture()), gs_texture_destroy};
				if (!alpha)
					throw std::runtime_error("Failed to wrap shared mask texture");
				alphaTexture_ = std::move(alpha);
				maskRefiner_ = std::move(refiner);
				engine_ = std::move(engine);
				appliedMaskSettings_.reset();
				if (engine_->runAsync(inputTexture_.get()) !=
				    MediaPipeLandscapeDirectMLEngine::RunResult::Submitted) {
					throw std::runtime_error("New DirectML engine did not accept its first input");
				}
			}
		} catch (const std::exception &exception) {
			blog(LOG_ERROR, OBS_LOG_HEADER "GPU image inference failed: %s", exception.what());
			return;
		}
	}
	TraceLoggingWrite(provider, "InferenceDecision", TraceLoggingLevel(5), TraceLoggingKeyword(0x1),
			  TraceLoggingBool(!skipMaskInference, "Required"));

	// The current source draw is independent of the submitted model output.
	{
		TraceLoggingThreadActivity<provider, 0x1> independent;
		TraceLoggingWriteStart(independent, "Pipeline.IndependentRendering");
		gs_set_render_target_with_color_space(sourceTexture, nullptr, GS_CS_709_EXTENDED);
		gs_ortho(0.0F, static_cast<float>(sourceWidth), 0.0F, static_cast<float>(sourceHeight), -100.0F,
			 100.0F);
		gs_set_viewport(0, 0, static_cast<int>(width_), static_cast<int>(height_));
		gs_clear(GS_CLEAR_COLOR, &zero, 0.0F, 0);
		while (gs_effect_loop(effect, "Draw")) {
			obs_source_video_render(target);
		}
	}

	// Submit dependent mask work on the model queue; wait only at the drawing deadline.
	const auto expansion = static_cast<std::int32_t>(std::clamp<std::int64_t>(unsafeRefineMaskExpansion, -30, 30));
	const auto maskSettings = std::tuple{refineMaskEnabled, refineMaskThreshold, std::int64_t(expansion)};
	if (!skipMaskInference || appliedMaskSettings_ != maskSettings) {
		bool modelResultPending = !skipMaskInference;
		try {
			{
				TraceLoggingThreadActivity<provider, 0x1> refine;
				TraceLoggingWriteStart(refine, "Pipeline.RefineMask");
				maskRefiner_->runAsync(skipMaskInference ? nullptr : engine_->gpuOutput(),
						       {refineMaskEnabled, std::clamp(refineMaskThreshold, 0.0F, 1.0F),
							expansion});
			}
			const bool completedBeforeDeadline = maskRefiner_->isDone();
			TraceLoggingWrite(provider, "InferenceReadiness", TraceLoggingLevel(5),
					  TraceLoggingKeyword(0x1),
					  TraceLoggingBool(completedBeforeDeadline, "CompletedBeforeDeadline"));
			if (!completedBeforeDeadline) {
				TraceLoggingThreadActivity<provider, 0x1> waiting;
				TraceLoggingWriteStart(waiting, "Pipeline.GpuOutputWait");
				maskRefiner_->blockCpuUntilComplete();
			}
			if (modelResultPending) {
				engine_->releaseGpuResult();
				modelResultPending = false;
			}
			maskRefiner_->complete();
			appliedMaskSettings_ = maskSettings;
		} catch (const std::exception &exception) {
			// Failed validation must not leave a completed model result unconsumed.
			if (modelResultPending) {
				try {
					engine_->blockCpuUntilInferenceCompletes();
					engine_->releaseGpuResult();
				} catch (const std::exception &cleanupError) {
					blog(LOG_ERROR, OBS_LOG_HEADER "GPU result cleanup failed: %s",
					     cleanupError.what());
				}
			}
			blog(LOG_ERROR, OBS_LOG_HEADER "GPU mask refinement failed: %s", exception.what());
			return;
		}
	}
	if (!skipMaskInference)
		std::swap(lastInputTexture_, inputTexture_);

	gs_texture_t *const alphaTexture = alphaTexture_.get();

	gs_texture_t *blurredTexture = sourceTexture;
	if (blurBackgroundEnabled) {
		const auto iterations = std::clamp<std::int64_t>(unsafeBlurBackgroundFactor, 0, 30);
		gs_ortho(0.0F, static_cast<float>(width_), 0.0F, static_cast<float>(height_), -100.0F, 100.0F);
		gs_set_viewport(0, 0, static_cast<int>(width_), static_cast<int>(height_));
		for (std::int64_t iteration = 0; iteration < iterations; ++iteration) {
			gs_texture_t *destination = blurTextures[iteration % blurTextures.size()];
			gs_set_render_target_with_color_space(destination, nullptr, GS_CS_709_EXTENDED);
			while (gs_effect_loop(effect, "DrawFocalBlur")) {
				gs_effect_set_texture(epImage, blurredTexture);
				gs_effect_set_texture(gs_effect_get_param_by_name(effect, "focalmask"), alphaTexture);
				gs_effect_set_float(gs_effect_get_param_by_name(effect, "xOffset"),
						    (static_cast<float>(iteration) + 0.5F) / width_);
				gs_effect_set_float(gs_effect_get_param_by_name(effect, "yOffset"),
						    (static_cast<float>(iteration) + 0.5F) / height_);
				gs_effect_set_int(gs_effect_get_param_by_name(effect, "blurIter"),
						  static_cast<int>(iteration));
				gs_effect_set_int(gs_effect_get_param_by_name(effect, "blurTotal"),
						  static_cast<int>(iterations));
				gs_effect_set_float(gs_effect_get_param_by_name(effect, "blurFocusPoint"),
						    blurFocusPoint);
				gs_effect_set_float(gs_effect_get_param_by_name(effect, "blurFocusDepth"),
						    blurFocusDepth);
				gs_draw_sprite(nullptr, 0, width_, height_);
			}
			blurredTexture = destination;
		}
	}

	frameBlurBackgroundEnabled_ = blurBackgroundEnabled;
	frameBlurredTexture_ = blurredTexture;
	filterState_ = FilterState::ReadyForRendering;

	// MARK: - Render to destination

	renderStatePop();
	renderFrameResult();
}

void MediaPipeLandscapeDirectMLPipeline::renderFrameResult() const noexcept
{
	gs_effect_t *effect = effect_.get();
	gs_texture_t *sourceTexture = sourceTexture_.get();
	gs_texture_t *alphaTexture = alphaTexture_.get();
	gs_eparam_t *epImage = gs_effect_get_param_by_name(effect, "image");
	gs_eparam_t *epAlphaMask = gs_effect_get_param_by_name(effect, "alphamask");
	gs_eparam_t *epBlurredBackground = gs_effect_get_param_by_name(effect, "blurredBackground");
	if (frameBlurBackgroundEnabled_) {
		while (gs_effect_loop(effect, "DrawWithFocalBlur")) {
			gs_effect_set_texture(epImage, sourceTexture);
			gs_effect_set_texture(epAlphaMask, alphaTexture);
			gs_effect_set_texture(epBlurredBackground, frameBlurredTexture_);
			gs_draw_sprite(sourceTexture, 0, 0, 0);
		}
	} else {
		while (gs_effect_loop(effect, "DrawWithoutBlur")) {
			gs_effect_set_texture(epImage, sourceTexture);
			gs_effect_set_texture(epAlphaMask, alphaTexture);
			gs_draw_sprite(sourceTexture, 0, 0, 0);
		}
	}
}

} // namespace BackgroundRemoval
