// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MediaPipeLandscapeDirectMLPipeline.hpp"
#include "BackgroundRemoval/MainEffect.hpp"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>

namespace BackgroundRemoval {

auto makeMediaPipeLandscapeDmlProgram() -> std::unique_ptr<BaselineDmlProgram>;

MediaPipeLandscapeDirectMLPipeline::MediaPipeLandscapeDirectMLPipeline(const FilterProperty &property,
								       obs_source_t *source,
								       MainEffect &backgroundRemovalEffect,
								       std::uint32_t width, std::uint32_t height)
	: program_(nullptr),
	  width_(width),
	  height_(height),
	  source_(source),
	  backgroundRemovalEffect_(backgroundRemovalEffect),
	  backgroundMask_(static_cast<std::size_t>(width) * height, 255),
	  modelMask_(modelPixelCount),
	  smoothedMask_(modelPixelCount),
	  lastInput_(modelPixelCount * 3),
	  modelInput_(modelPixelCount * 3),
	  modelOutput_(modelPixelCount),
	  smooth_({modelWidth, modelHeight}),
	  modelThreshold_({modelWidth, modelHeight}),
	  outputThreshold_({width, height}),
	  outputMorphology_({width, height})
{
	using namespace ObsBridgeUtils;
	if (!source || width == 0 || height == 0) {
		throw std::invalid_argument("Invalid MediaPipe Landscape DirectML pipeline arguments");
	}
	program_ = makeMediaPipeLandscapeDmlProgram();
	GsUnique::GraphicsGuard graphicsGuard;
	try {
		sourceTexture_ = makeUniqueTexture(width, height, GS_BGRA, GS_RENDER_TARGET);
		blurTextures_[0] = makeUniqueTexture(width, height, GS_BGRA, GS_RENDER_TARGET);
		blurTextures_[1] = makeUniqueTexture(width, height, GS_BGRA, GS_RENDER_TARGET);
		modelInputTexture_ = makeUniqueTexture(modelWidth, modelHeight * 3, GS_R32F, GS_RENDER_TARGET);
		alphaTexture_ = makeUniqueTexture(width, height, GS_R8, GS_DYNAMIC);
		textureReader_ = std::make_unique<ObsBridgeUtils::TextureReader>(modelWidth, modelHeight * 3, GS_R32F);
		if (!sourceTexture_ || !blurTextures_[0] || !blurTextures_[1] || !modelInputTexture_ ||
		    !alphaTexture_) {
			throw std::runtime_error("Failed to create MediaPipe Landscape rendering textures");
		}
	} catch (...) {
		textureReader_.reset();
		alphaTexture_.reset();
		modelInputTexture_.reset();
		blurTextures_ = {};
		sourceTexture_.reset();
		GsUnique::drain();
		throw;
	}
	update(property);
}

MediaPipeLandscapeDirectMLPipeline::~MediaPipeLandscapeDirectMLPipeline() noexcept
{
	ObsBridgeUtils::GsUnique::GraphicsGuard graphicsGuard;
	textureReader_.reset();
	alphaTexture_.reset();
	modelInputTexture_.reset();
	blurTextures_ = {};
	sourceTexture_.reset();
	ObsBridgeUtils::GsUnique::drain();
}

auto MediaPipeLandscapeDirectMLPipeline::processImage(const std::uint8_t *inputData, std::uint32_t linesize,
						      std::uint8_t *output) noexcept -> bool
{
	if (!inputData || !output || linesize < modelWidth * sizeof(float)) {
		return false;
	}
	try {
		for (std::uint32_t channel = 0; channel < 3; ++channel) {
			for (std::uint32_t row = 0; row < modelHeight; ++row) {
				const auto *source = reinterpret_cast<const float *>(
					inputData + static_cast<std::size_t>(channel * modelHeight + row) * linesize);
				std::memcpy(modelInput_.data() +
						    static_cast<std::size_t>(channel * modelHeight + row) * modelWidth,
					    source, modelWidth * sizeof(float));
			}
		}
		program_->run(modelInput_, modelOutput_);
		for (std::size_t index = 0; index < modelPixelCount; ++index) {
			output[index] = static_cast<std::uint8_t>(
				std::clamp((1.0F - modelOutput_[index]) * 255.0F, 0.0F, 255.0F));
		}
		return true;
	} catch (const std::exception &exception) {
		blog(LOG_ERROR, OBS_LOG_HEADER "MediaPipe Landscape DirectML inference failed: %s", exception.what());
		return false;
	}
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
	refineMaskContour_.store(static_cast<float>(property.refineMaskContour), std::memory_order_relaxed);
	refineMaskExpansion_.store(property.refineMaskExpansion, std::memory_order_relaxed);
	maskSettingsDirty_.store(true, std::memory_order_relaxed);
}

auto MediaPipeLandscapeDirectMLPipeline::shouldInferMask(const ObsBridgeUtils::TextureReader *textureReader,
							 float similarityThreshold) const noexcept -> bool
{
	if (!hasLastInput_) {
		return true;
	}
	const auto &buffer = textureReader->getBuffer();
	const std::uint32_t linesize = textureReader->getBufferLinesize();
	const double psnr = psnr_.calculate(reinterpret_cast<const float *>(buffer.data()),
					    {modelWidth, modelHeight * 3, linesize}, lastInput_.data(),
					    {modelWidth, modelHeight * 3, modelWidth * sizeof(float)});
	return psnr <= similarityThreshold;
}

auto MediaPipeLandscapeDirectMLPipeline::inferMask(const ObsBridgeUtils::TextureReader *textureReader) noexcept -> bool
{
	const auto &buffer = textureReader->getBuffer();
	const std::uint32_t linesize = textureReader->getBufferLinesize();
	if (linesize < modelWidth * sizeof(float) ||
	    buffer.size() < static_cast<std::size_t>(linesize) * modelHeight * 3) {
		blog(LOG_ERROR, OBS_LOG_HEADER "MediaPipe Landscape model input has an invalid layout");
		return false;
	}

	for (std::uint32_t row = 0; row < modelHeight * 3; ++row) {
		std::memcpy(lastInput_.data() + static_cast<std::size_t>(row) * modelWidth,
			    buffer.data() + static_cast<std::size_t>(row) * linesize, modelWidth * sizeof(float));
	}
	const bool inferenceSucceeded = processImage(buffer.data(), linesize, modelMask_.data());
	if (!inferenceSucceeded) {
		blog(LOG_ERROR, OBS_LOG_HEADER "MediaPipe Landscape DirectML inference failed");
		hasLastInput_ = false;
		return false;
	}
	hasLastInput_ = true;
	return true;
}

auto MediaPipeLandscapeDirectMLPipeline::refinementFailed(const char *message) noexcept -> bool
{
	blog(LOG_ERROR, OBS_LOG_HEADER "Background-mask refinement failed: %s", message);
	hasLastInput_ = false;
	return false;
}

auto MediaPipeLandscapeDirectMLPipeline::refineMask(bool refineMaskEnabled, float refineMaskThreshold,
						    float refineMaskContour, std::int64_t refineMaskExpansion) noexcept
	-> bool
{
	auto &mask = modelMask_;
	auto &resizedMask = backgroundMask_;
	const auto modelThresholdValue =
		static_cast<std::uint8_t>((1.0F - std::clamp(refineMaskThreshold, 0.0F, 1.0F)) * 255.0F);

	bool modelThresholdSucceeded = true;
	if (refineMaskEnabled) {
		modelThresholdSucceeded =
			modelThreshold_.apply(mask.data(), modelWidth, mask.data(), modelWidth, modelThresholdValue);
	}
	if (!modelThresholdSucceeded) {
		return refinementFailed("MediaPipe Landscape model-mask threshold failed");
	}
	bool componentFilteringSucceeded = true;
	if (refineMaskEnabled && refineMaskContour > 0.0F && refineMaskContour < 1.0F) {
		componentFilteringSucceeded =
			removeSmallComponents_.apply(mask.data(), {modelWidth, modelHeight, modelWidth}, mask.data(),
						     {modelWidth, modelHeight, modelWidth},
						     static_cast<double>(modelPixelCount) * refineMaskContour);
	}
	if (!componentFilteringSucceeded) {
		return refinementFailed("MediaPipe Landscape connected-component filtering failed");
	}
	bool smoothingSucceeded = true;
	if (refineMaskEnabled) {
		smoothingSucceeded = smooth_.apply(mask.data(), modelWidth, smoothedMask_.data(), modelWidth);
	}
	if (!smoothingSucceeded) {
		return refinementFailed("MediaPipe Landscape mask smoothing failed");
	}
	if (refineMaskEnabled) {
		mask.swap(smoothedMask_);
	}
	const bool resizeSucceeded = resize_.apply(mask.data(), {modelWidth, modelHeight, modelWidth},
						   resizedMask.data(), {width_, height_, width_});
	if (!resizeSucceeded) {
		return refinementFailed("MediaPipe Landscape mask resize failed");
	}
	bool outputThresholdSucceeded = true;
	if (refineMaskEnabled) {
		outputThresholdSucceeded =
			outputThreshold_.apply(resizedMask.data(), width_, resizedMask.data(), width_, 128);
	}
	if (!outputThresholdSucceeded) {
		return refinementFailed("MediaPipe Landscape output-mask threshold failed");
	}
	bool erosionSetupSucceeded = true;
	if (refineMaskEnabled && refineMaskExpansion > 0) {
		erosionSetupSucceeded = outputMorphology_.begin(resizedMask.data(), width_);
	}
	if (!erosionSetupSucceeded) {
		return refinementFailed("MediaPipe Landscape mask erosion setup failed");
	}
	for (std::int64_t iteration = 0; refineMaskEnabled && iteration < refineMaskExpansion; ++iteration) {
		const bool erosionSucceeded = outputMorphology_.erode();
		if (!erosionSucceeded) {
			return refinementFailed("MediaPipe Landscape mask erosion failed");
		}
	}
	bool erosionOutputSucceeded = true;
	if (refineMaskEnabled && refineMaskExpansion > 0) {
		erosionOutputSucceeded = outputMorphology_.end(resizedMask.data(), width_);
	}
	if (!erosionOutputSucceeded) {
		return refinementFailed("MediaPipe Landscape mask erosion output failed");
	}
	bool dilationSetupSucceeded = true;
	if (refineMaskEnabled && refineMaskExpansion < 0) {
		dilationSetupSucceeded = outputMorphology_.begin(resizedMask.data(), width_);
	}
	if (!dilationSetupSucceeded) {
		return refinementFailed("MediaPipe Landscape mask dilation setup failed");
	}
	for (std::int64_t iteration = 0; refineMaskEnabled && iteration > refineMaskExpansion; --iteration) {
		const bool dilationSucceeded = outputMorphology_.dilate();
		if (!dilationSucceeded) {
			return refinementFailed("MediaPipe Landscape mask dilation failed");
		}
	}
	bool dilationOutputSucceeded = true;
	if (refineMaskEnabled && refineMaskExpansion < 0) {
		dilationOutputSucceeded = outputMorphology_.end(resizedMask.data(), width_);
	}
	if (!dilationOutputSucceeded) {
		return refinementFailed("MediaPipe Landscape mask dilation output failed");
	}
	return true;
}

void MediaPipeLandscapeDirectMLPipeline::videoTick(float) noexcept {}

auto MediaPipeLandscapeDirectMLPipeline::blurBackground(gs_texture_t *sourceTexture, gs_texture_t *alphaTexture,
							const std::array<gs_texture_t *, 2> &blurTextures,
							std::size_t iterations, float focusPoint,
							float focusDepth) noexcept -> gs_texture_t *
{
	gs_texture_t *blurredTexture = sourceTexture;
	for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
		gs_texture_t *destination = blurTextures[iteration % blurTextures.size()];
		backgroundRemovalEffect_.focalBlur(destination, blurredTexture, alphaTexture,
						   static_cast<std::int64_t>(iteration),
						   static_cast<std::int64_t>(iterations), focusPoint, focusDepth);
		blurredTexture = destination;
	}
	return blurredTexture;
}

void MediaPipeLandscapeDirectMLPipeline::videoRender(gs_effect_t *) noexcept
{
	// Skip rendering while the source is unavailable.
	const bool sourceEnabled = obs_source_enabled(source_);
	if (!sourceEnabled) {
		obs_source_skip_video_filter(source_);
		return;
	}

	// Snapshot all externally writable settings for this frame.
	const bool blurBackgroundEnabled = blurBackgroundEnabled_.load(std::memory_order_relaxed);
	const std::int64_t unsafeBlurBackgroundFactor = blurBackgroundFactor_.load(std::memory_order_relaxed);
	const float blurFocusPoint = blurFocusPoint_.load(std::memory_order_relaxed);
	const float blurFocusDepth = blurFocusDepth_.load(std::memory_order_relaxed);
	const float imageSimilarityThreshold = imageSimilarityThreshold_.load(std::memory_order_relaxed);
	const bool refineMaskEnabled = refineMaskEnabled_.load(std::memory_order_relaxed);
	const float refineMaskThreshold = refineMaskThreshold_.load(std::memory_order_relaxed);
	const float refineMaskContour = refineMaskContour_.load(std::memory_order_relaxed);
	const std::int64_t unsafeRefineMaskExpansion = refineMaskExpansion_.load(std::memory_order_relaxed);

	// Snapshot owned rendering resources as raw pointers for this frame.
	gs_texture_t *const sourceTexture = sourceTexture_.get();
	gs_texture_t *const modelInputTexture = modelInputTexture_.get();
	gs_texture_t *const alphaTexture = alphaTexture_.get();
	const std::array<gs_texture_t *, 2> blurTextures{blurTextures_[0].get(), blurTextures_[1].get()};
	ObsBridgeUtils::TextureReader *const textureReader = textureReader_.get();

	// Bound externally writable loop counts to protect render-thread availability.
	const std::size_t blurBackgroundIterations =
		static_cast<std::size_t>(std::clamp<std::int64_t>(unsafeBlurBackgroundFactor, 0, 20));
	const std::int64_t refineMaskExpansion = std::clamp<std::int64_t>(unsafeRefineMaskExpansion, -30, 30);

	// Select the source filtered by this plugin.
	obs_source_t *const target = obs_filter_get_target(source_);
	if (!target) {
		obs_source_skip_video_filter(source_);
		return;
	}

	// Render the current source frame into the reusable texrender.
	backgroundRemovalEffect_.renderSource(sourceTexture, target);

	// Extract the model's scaled RGB planes from the captured source frame.
	backgroundRemovalEffect_.extractScaledRGBPlanes(modelInputTexture, sourceTexture);

	// Read the current model input texture back to CPU memory.
	const bool modelInputRead = textureReader->read(modelInputTexture);
	if (!modelInputRead) {
		blog(LOG_ERROR, OBS_LOG_HEADER "Model-input readback failed");
		hasLastInput_ = false;
		obs_source_skip_video_filter(source_);
		return;
	}

	// Determine which model operations are required for this frame.
	const bool maskSettingsChanged = maskSettingsDirty_.exchange(false, std::memory_order_relaxed);
	const bool requiresMaskInference = maskSettingsChanged ||
					   shouldInferMask(textureReader, imageSimilarityThreshold);

	// Infer a new model-resolution mask when the model input changed.
	bool maskInferenceSucceeded = true;
	if (requiresMaskInference) {
		maskInferenceSucceeded = inferMask(textureReader);
	}
	if (!maskInferenceSucceeded) {
		obs_source_skip_video_filter(source_);
		return;
	}

	// Refine the inferred mask into the output-resolution background mask.
	bool maskRefinementSucceeded = true;
	if (requiresMaskInference) {
		maskRefinementSucceeded =
			refineMask(refineMaskEnabled, refineMaskThreshold, refineMaskContour, refineMaskExpansion);
	}
	if (!maskRefinementSucceeded) {
		obs_source_skip_video_filter(source_);
		return;
	}

	// Upload the CPU background mask to the GPU alpha texture.
	gs_texture_set_image(alphaTexture, backgroundMask_.data(), width_, false);

	// Generate the blurred background texture when background blur is enabled.
	gs_texture_t *blurredBackgroundTexture = sourceTexture;
	if (blurBackgroundEnabled) {
		blurredBackgroundTexture = blurBackground(sourceTexture, alphaTexture, blurTextures,
							  blurBackgroundIterations, blurFocusPoint, blurFocusDepth);
	}

	// Composite the source and background using the uploaded alpha mask.
	gs_blend_state_push();
	gs_reset_blend_state();
	if (blurBackgroundEnabled) {
		backgroundRemovalEffect_.directDrawWithBlurredBackground(sourceTexture, alphaTexture,
									 blurredBackgroundTexture);
	} else {
		backgroundRemovalEffect_.directDrawWithoutBlur(sourceTexture, alphaTexture);
	}
	gs_blend_state_pop();
}

} // namespace BackgroundRemoval
