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

#include "MPLDPipeline.hpp"

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

#include "MPLDEngine.hpp"

TRACELOGGING_DECLARE_PROVIDER(g_hProvider);

namespace {

using InitError = BackgroundRemoval::MPLDPipeline::InitError;

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

MPLDPipeline::MPLDPipeline(const FilterProperty &property,
								       obs_source_t *source, std::uint32_t width,
								       std::uint32_t height)
	: engine_{std::make_unique<MPLDEngine>(MPLDEngine::Output::GpuMask,
								     width, height)},
	  width_(width),
	  height_(height),
	  source_(source),
	  renderingEffect_{gs_effect_create_unique(MPLDDetail::renderingEffectString, "MPLDRendering.effect")},
	  epImage_{gs_effect_get_param_by_name(renderingEffect_.get(), "image")},
	  epImage1_{gs_effect_get_param_by_name(renderingEffect_.get(), "image1")},
	  epImage2_{gs_effect_get_param_by_name(renderingEffect_.get(), "image2")},
	  gateEffect_{gs_effect_create_unique(MPLDDetail::gateEffectString, "MPLDGate.effect")},
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

MPLDPipeline::~MPLDPipeline() noexcept
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

auto MPLDPipeline::autoSetSensitivity() noexcept -> bool
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

void MPLDPipeline::update(const FilterProperty &property) noexcept
{
	blurBackgroundEnabled_.store(property.blurBackgroundEnabled, std::memory_order_relaxed);
	blurBackgroundFactor_.store(property.blurBackgroundFactor, std::memory_order_relaxed);
	blurFocusPoint_.store(static_cast<float>(property.blurFocusPoint), std::memory_order_relaxed);
	blurFocusDepth_.store(static_cast<float>(property.blurFocusDepth), std::memory_order_relaxed);
	imageSimilarityThreshold_.store(static_cast<float>(property.imageSimilarityThreshold),
					std::memory_order_relaxed);
	refineMaskEnabled_.store(property.refineMaskEnabled, std::memory_order_relaxed);
	refineMaskThreshold_.store(static_cast<float>(property.refineMaskThreshold), std::memory_order_relaxed);
}

void MPLDPipeline::videoTick(float) noexcept
{
	if (differenceStaged_) {
		TraceLoggingThreadActivity<g_hProvider, 0x1> trace;
		TraceLoggingWriteStart(trace, "MPLDPipeline.ReadsDifference");
		bool valid = false;
		std::uint64_t sum = 0;
		static_assert(std::uint64_t(kModelWidth) * kModelHeight <=
			      std::numeric_limits<decltype(sum)>::max() / 255);
		std::uint8_t *data = nullptr;
		std::uint32_t linesize = 0;
		obs_enter_graphics();
		if (lumaAbsDiffStagesurf_ && gs_stagesurface_map(lumaAbsDiffStagesurf_.get(), &data, &linesize)) {
			if (data && linesize >= kModelWidth) {
				for (std::uint32_t y = 0; y < kModelHeight; ++y) {
					const auto *row = data + std::size_t(y) * linesize;
					for (std::uint32_t x = 0; x < kModelWidth; ++x)
						sum += row[x];
				}
				valid = true;
			}
			gs_stagesurface_unmap(lumaAbsDiffStagesurf_.get());
		}
		obs_leave_graphics();
		differenceStaged_ = false;
		if (valid) {
			differenceHistory_.record(differenceTimestamp_, double(sum) / kModelInputMaximumSum);
			const double sensitivity = imageSimilarityThreshold_.load(std::memory_order_relaxed);
			bool motion = !std::isfinite(sensitivity);
			if (!motion) {
				const auto threshold = static_cast<std::uint64_t>(std::clamp(
					std::ceil(kModelInputMaximumSum * std::pow(10.0, -sensitivity / 20.0)),
					0.0, kModelInputMaximumSum + 1));
				motion = sum != 0 && sum >= threshold;
			}
			if (motion)
				remainingInferenceFrames_ = 1 + additionalInferenceFrames_.load(std::memory_order_relaxed);
		} else {
			blog(LOG_ERROR, OBS_LOG_HEADER "MPLDPipeline.ReadDifferenceError");
			remainingInferenceFrames_ = std::max(remainingInferenceFrames_, 1u);
		}
	}
	filterState_ = FilterState::NeedsProcessing;
}

auto MPLDPipeline::needsRecreation() const noexcept -> bool
{
	return needsRecreation_;
}

void MPLDPipeline::videoRender(gs_effect_t *) noexcept
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
	const bool refineMaskEnabled = refineMaskEnabled_.load(std::memory_order_relaxed);
	const float refineMaskThreshold = refineMaskThreshold_.load(std::memory_order_relaxed);
	const auto maskSettings = std::tuple{refineMaskEnabled, refineMaskThreshold};

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
		TraceLoggingWriteStart(trace, "MPLDPipeline.RendersInputTexture");
		gs_set_render_target_with_color_space(inputTexture, nullptr, GS_CS_709_EXTENDED);
		gs_ortho(0, kModelWidth, 0, kModelHeight, -100, 100);
		gs_set_viewport(0, 0, kModelWidth, kModelHeight);
		gs_clear(GS_CLEAR_COLOR, &zero, 0, 0);
		while (gs_effect_loop(gateEffect_.get(), "Draw")) {
			gs_effect_set_texture(gateImage_, targetTexture);
			gs_draw_sprite(nullptr, 0, kModelWidth, kModelHeight);
		}
	}

	if (willGenerateMask && inputTexture && lastInputTexture_ && lumaAbsDiffTexture_ && lumaAbsDiffStagesurf_) {
		TraceLoggingThreadActivity<g_hProvider, 0x1> trace;
		TraceLoggingWriteStart(trace, "MPLDPipeline.StagesDifference");
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
	const bool willCompleteMask = willInferMask ||
		(willGenerateMask && appliedMaskSettings_ && *appliedMaskSettings_ != maskSettings);
	if (willCompleteMask) {
		bool skip = false;
		if (!engine_ || !alphaTexture || !inputTexture) {
			blog(LOG_ERROR, OBS_LOG_HEADER "MPLDPipeline.InfersMaskNullError");
			skip = true;
		}
		if (!skip && willInferMask) {
			if (HRESULT hr = engine_->runAsync(inputTexture, refineMaskEnabled,
				std::clamp(refineMaskThreshold, 0.0F, 1.0F)); FAILED(hr)) {
				blog(LOG_ERROR, OBS_LOG_HEADER "MPLDPipeline.RunsInferenceError: %ld", hr);
				willRecreateEngine = engine_->needsRecreation();
				skip = true;
			}
		}
		if (!skip && !willInferMask) {
			if (HRESULT hr = engine_->refineAsync(refineMaskEnabled, std::clamp(refineMaskThreshold, 0.0F, 1.0F)); FAILED(hr)) {
				blog(LOG_ERROR, OBS_LOG_HEADER "MPLDPipeline.RefineMaskError: %ld", hr);
				skip = true;
			}
		}
		if (!skip) {
			TraceLoggingThreadActivity<g_hProvider, 0x1> waiting;
			TraceLoggingWriteStart(waiting, "Pipeline.GpuOutputWait");
			if (HRESULT hr = engine_->blockCpuUntilSubmittedGpuWorkCompletes(); FAILED(hr)) {
				blog(LOG_ERROR, OBS_LOG_HEADER "MPLDPipeline.WaitInferenceError: %ld", hr);
				skip = true;
			}
		}
		if (!skip) {
			if (HRESULT hr = engine_->complete(); FAILED(hr)) {
				blog(LOG_ERROR, OBS_LOG_HEADER "MPLDPipeline.CompleteInferenceError: %ld", hr);
				skip = true;
			}
		}
		if (!skip) {
			appliedMaskSettings_ = maskSettings;
			if (willInferMask) {
				--remainingInferenceFrames_;
			}
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
			blog(LOG_ERROR, OBS_LOG_HEADER "MPLDPipeline.BlursBackgroundNullError: destination");
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
}

} // namespace BackgroundRemoval
