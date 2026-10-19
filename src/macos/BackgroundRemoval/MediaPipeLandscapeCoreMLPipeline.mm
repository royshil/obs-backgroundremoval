// SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
// SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MediaPipeLandscapeCoreMLPipeline.hpp"
#include "Effects/BackgroundRemovalEffect.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <stdexcept>

#include <ObsBridgeUtils/GsUnique.hpp>

#include "Memory/AlignedBuffer.hpp"

#include <Accelerate/Accelerate.h>

#import <CoreML/CoreML.h>
#import <Foundation/Foundation.h>

namespace BackgroundRemoval {

auto MediaPipeLandscapeCoreMLPipeline::osLogger() noexcept -> os_log_t
{
	static os_log_t logger = os_log_create("com.royshil.obs-backgroundremoval", "MediaPipeLandscapeCoreMLPipeline");
	return logger;
}

class MediaPipeLandscapeCoreMLPipeline::CoreMLState final {
public:
	MLModel *model = nil;
	MLMultiArray *input = nil;
	MLMultiArray *output = nil;
	MLDictionaryFeatureProvider *features = nil;
	MLPredictionOptions *predictionOptions = nil;
};

void MediaPipeLandscapeCoreMLPipeline::initializePipeline()
{
	@autoreleasepool {
		char *modelPath = obs_module_file("MediaPipeLandscape.mlmodelc");
		if (!modelPath) {
			throw std::runtime_error("MediaPipe Landscape Core ML model was not found");
		}
		NSString *path = [NSString stringWithUTF8String:modelPath];
		bfree(modelPath);
		MLModelConfiguration *configuration = [[MLModelConfiguration alloc] init];
		configuration.computeUnits = MLComputeUnitsCPUAndNeuralEngine;
		NSError *error = nil;
		coreMLState_->model = [MLModel modelWithContentsOfURL:[NSURL fileURLWithPath:path]
							configuration:configuration
								error:&error];
		if (!coreMLState_->model) {
			throw std::runtime_error(error.localizedDescription.UTF8String
							 ?: "Failed to load Core ML model");
		}
		coreMLState_->input = [[MLMultiArray alloc] initWithShape:@[@1, @3, @144, @256]
								 dataType:MLMultiArrayDataTypeFloat16
								    error:&error];
		if (!coreMLState_->input) {
			throw std::runtime_error(error.localizedDescription.UTF8String
							 ?: "Failed to allocate Core ML input");
		}
		coreMLState_->features = [[MLDictionaryFeatureProvider alloc] initWithDictionary:@{
			@"pixel_values": [MLFeatureValue featureValueWithMultiArray:coreMLState_->input]
		}
											   error:&error];
		if (!coreMLState_->features) {
			throw std::runtime_error(error.localizedDescription.UTF8String
							 ?: "Failed to create Core ML input features");
		}
		coreMLState_->output = [[MLMultiArray alloc] initWithShape:@[@1, @1, @144, @256]
								  dataType:MLMultiArrayDataTypeFloat16
								     error:&error];
		if (!coreMLState_->output) {
			throw std::runtime_error(error.localizedDescription.UTF8String
							 ?: "Failed to allocate Core ML output");
		}
		coreMLState_->predictionOptions = [[MLPredictionOptions alloc] init];
		coreMLState_->predictionOptions.outputBackings = @{@"alphas": coreMLState_->output};
	}
}

auto MediaPipeLandscapeCoreMLPipeline::processImage(const std::uint8_t *modelInput, float *outputBuffer) noexcept
	-> bool
{
	@autoreleasepool {
		const auto *newInput = reinterpret_cast<const _Float16 *>(modelInput);
		auto *inputData = static_cast<_Float16 *>(coreMLState_->input.dataPointer);
		std::memcpy(inputData, newInput, modelInputElementCount * sizeof(_Float16));
		hasLastImage_ = true;

		NSError *error = nil;
		id<MLFeatureProvider> prediction = [coreMLState_->model
			predictionFromFeatures:coreMLState_->features
				       options:coreMLState_->predictionOptions
					 error:&error];
		if (!prediction) {
			const char *message = error.localizedDescription.UTF8String ?: "Core ML inference failed";
			blog(LOG_ERROR, OBS_LOG_HEADER "Background-removal inference failed: %s", message);
			os_log_with_type(osLogger(), OS_LOG_TYPE_ERROR,
					 "Background-removal inference failed: %{public}s", message);
			return false;
		}
		const auto *outputData = static_cast<const _Float16 *>(coreMLState_->output.dataPointer);
		for (std::size_t index = 0; index < maskPixelCount; ++index) {
			outputBuffer[index] = 1.0F - static_cast<float>(outputData[index]);
		}
		return true;
	}
}

auto MediaPipeLandscapeCoreMLPipeline::isInitialized() const noexcept -> bool
{
	return coreMLState_->model != nil;
}

MediaPipeLandscapeCoreMLPipeline::MediaPipeLandscapeCoreMLPipeline(
	const FilterProperty &property, obs_source_t *source, Effects::BackgroundRemovalEffect &backgroundRemovalEffect,
	std::uint32_t width, std::uint32_t height)
	: width_(width),
	  height_(height),
	  source_(source),
	  backgroundRemovalEffect_(backgroundRemovalEffect),
	  backgroundMask_(Memory::makeAlignedBytes(static_cast<std::size_t>(width) * height)),
	  outputMask_(Memory::makeAlignedBytes(static_cast<std::size_t>(width) * height)),
	  inferenceOutput_(Memory::makeAlignedBytes(maskPixelCount * sizeof(float))),
	  inferenceMask_(Memory::makeAlignedBytes(maskPixelCount)),
	  componentMask_(Memory::makeAlignedBytes(maskPixelCount)),
	  smoothingMask_(Memory::makeAlignedBytes(maskPixelCount)),
	  smooth_({maskWidth, maskHeight}),
	  modelThreshold_({maskWidth, maskHeight}),
	  outputThreshold_({width, height}),
	  outputMorphology_({width, height}),
	  coreMLState_(std::make_unique<CoreMLState>())
{
	using namespace ObsBridgeUtils;

	if (!source) {
		throw std::invalid_argument("Background removal source is null");
	} else if (width == 0) {
		throw std::invalid_argument("Background removal width is zero");
	} else if (height == 0) {
		throw std::invalid_argument("Background removal height is zero");
	}
	std::fill_n(backgroundMask_.get(), static_cast<std::size_t>(width) * height, std::uint8_t{255});
	initializePipeline();

	GsUnique::GraphicsGuard graphicsGuard;
	try {
		textureReader_ = std::make_unique<TextureReader>(maskWidth, modelInputHeight, GS_R16F);
		sourceTexture_ = makeUniqueTexture(width, height, GS_BGRA, GS_RENDER_TARGET);
		modelInputTexture_ = makeUniqueTexture(maskWidth, modelInputHeight, GS_R16F, GS_RENDER_TARGET);
		blurTextures_[0] = makeUniqueTexture(width, height, GS_BGRA, GS_RENDER_TARGET);
		blurTextures_[1] = makeUniqueTexture(width, height, GS_BGRA, GS_RENDER_TARGET);
		alphaTexture_ = makeUniqueTexture(width, height, GS_R8, GS_DYNAMIC);
		if (!sourceTexture_ || !modelInputTexture_ || !blurTextures_[0] || !blurTextures_[1] ||
		    !alphaTexture_) {
			throw std::runtime_error("Failed to create rendering textures");
		}
	} catch (...) {
		textureReader_.reset();
		alphaTexture_.reset();
		blurTextures_ = {};
		modelInputTexture_.reset();
		sourceTexture_.reset();
		GsUnique::drain();
		throw;
	}

	update(property);
	blog(LOG_INFO, OBS_LOG_HEADER "MediaPipe Landscape rendering pipeline initialized (%ux%u)", width_, height_);
	os_log_with_type(osLogger(), OS_LOG_TYPE_INFO,
			 "MediaPipe Landscape rendering pipeline initialized (%{public}ux%{public}u)", width_, height_);
}

MediaPipeLandscapeCoreMLPipeline::~MediaPipeLandscapeCoreMLPipeline() noexcept
{
	using namespace ObsBridgeUtils;

	GsUnique::GraphicsGuard graphicsGuard;
	textureReader_.reset();
	alphaTexture_.reset();
	blurTextures_ = {};
	modelInputTexture_.reset();
	sourceTexture_.reset();
	GsUnique::drain();
}

void MediaPipeLandscapeCoreMLPipeline::update(const FilterProperty &property) noexcept
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

auto MediaPipeLandscapeCoreMLPipeline::shouldInferMask(const ObsBridgeUtils::TextureReader *textureReader,
						       float similarityThreshold) const noexcept -> bool
{
	if (!hasLastImage_) {
		return true;
	}
	const auto *newInput = reinterpret_cast<const _Float16 *>(textureReader->getBuffer().data());
	const auto *lastInput = static_cast<const _Float16 *>(coreMLState_->input.dataPointer);
	double squaredError = 0.0;
	for (std::size_t index = 0; index < modelInputElementCount; ++index) {
		const double difference = static_cast<double>(newInput[index]) - lastInput[index];
		squaredError += difference * difference;
	}
	const double meanSquaredError = squaredError / static_cast<double>(modelInputElementCount);
	const double psnr = meanSquaredError == 0.0 ? std::numeric_limits<double>::infinity()
						    : 10.0 * std::log10(1.0 / meanSquaredError);
	return psnr <= similarityThreshold;
}

auto MediaPipeLandscapeCoreMLPipeline::inferMask(const ObsBridgeUtils::TextureReader *textureReader) noexcept -> bool
{
	const auto &modelInput = textureReader->getBuffer();
	auto *inferenceOutputData = reinterpret_cast<float *>(inferenceOutput_.get());
	if (!processImage(modelInput.data(), inferenceOutputData)) {
		hasLastImage_ = false;
		return false;
	}
	return true;
}

auto MediaPipeLandscapeCoreMLPipeline::refinementFailed(const char *message) noexcept -> bool
{
	blog(LOG_ERROR, OBS_LOG_HEADER "Background-mask refinement failed: %s", message);
	os_log_with_type(osLogger(), OS_LOG_TYPE_ERROR, "Background-mask refinement failed: %{public}s", message);
	hasLastImage_ = false;
	return false;
}

auto MediaPipeLandscapeCoreMLPipeline::refineMask(bool refineMaskEnabled, float refineMaskThreshold,
						  float refineMaskContour, std::int64_t refineMaskExpansion) noexcept
	-> bool
{
	auto *inferenceOutputData = reinterpret_cast<float *>(inferenceOutput_.get());
	if (refineMaskEnabled) {
		refineMaskThreshold = 1.0F - std::clamp(refineMaskThreshold, 0.0F, 1.0F);
		if (!modelThreshold_.apply(inferenceOutputData, maskWidth * sizeof(float), inferenceMask_.get(),
					   maskWidth, refineMaskThreshold)) {
			return refinementFailed("MediaPipe Landscape float-mask threshold failed");
		}
	} else {
		const vImage_Buffer inferenceOutputBuffer{inferenceOutputData, maskHeight, maskWidth,
							  maskWidth * sizeof(float)};
		const vImage_Buffer inferenceMaskBuffer{inferenceMask_.get(), maskHeight, maskWidth, maskWidth};
		if (vImageConvert_PlanarFtoPlanar8(&inferenceOutputBuffer, &inferenceMaskBuffer, 1.0F, 0.0F,
						   kvImageNoFlags) != kvImageNoError) {
			return refinementFailed("vImage float-mask conversion failed");
		}
	}

	if (refineMaskEnabled) {
		if (refineMaskContour > 0.0F && refineMaskContour < 1.0F) {
			if (!removeSmallComponents_.apply(inferenceMask_.get(), {maskWidth, maskHeight, maskWidth},
							  componentMask_.get(), {maskWidth, maskHeight, maskWidth},
							  static_cast<double>(maskPixelCount) * refineMaskContour)) {
				return refinementFailed("Connected-component filtering failed");
			}
			inferenceMask_.swap(componentMask_);
		}
	}

	if (refineMaskEnabled) {
		if (!smooth_.apply(inferenceMask_.get(), maskWidth, smoothingMask_.get(), maskWidth)) {
			return refinementFailed("vImage mask smoothing failed");
		}
		if (!resize_.apply(smoothingMask_.get(), {maskWidth, maskHeight, maskWidth}, outputMask_.get(),
				   {width_, height_, width_})) {
			return refinementFailed("MediaPipe Landscape smoothed-mask scaling failed");
		}
		if (!outputThreshold_.apply(outputMask_.get(), width_, outputMask_.get(), width_, 128)) {
			return refinementFailed("MediaPipe Landscape output-mask threshold failed");
		}
	} else if (!resize_.apply(inferenceMask_.get(), {maskWidth, maskHeight, maskWidth}, outputMask_.get(),
				  {width_, height_, width_})) {
		return refinementFailed("MediaPipe Landscape mask scaling failed");
	}

	if (refineMaskEnabled) {
		if (refineMaskExpansion > 0) {
			if (!outputMorphology_.begin(outputMask_.get(), width_)) {
				return refinementFailed("MediaPipe Landscape mask erosion setup failed");
			}
			for (std::int64_t iteration = 0; iteration < refineMaskExpansion; ++iteration) {
				if (!outputMorphology_.erode()) {
					return refinementFailed("MediaPipe Landscape mask erosion failed");
				}
			}
			if (!outputMorphology_.end(outputMask_.get(), width_)) {
				return refinementFailed("MediaPipe Landscape mask erosion output failed");
			}
		} else if (refineMaskExpansion < 0) {
			if (!outputMorphology_.begin(outputMask_.get(), width_)) {
				return refinementFailed("MediaPipe Landscape mask dilation setup failed");
			}
			for (std::int64_t iteration = 0; iteration > refineMaskExpansion; --iteration) {
				if (!outputMorphology_.dilate()) {
					return refinementFailed("MediaPipe Landscape mask dilation failed");
				}
			}
			if (!outputMorphology_.end(outputMask_.get(), width_)) {
				return refinementFailed("MediaPipe Landscape mask dilation output failed");
			}
		}
	}
	backgroundMask_.swap(outputMask_);
	return true;
}

void MediaPipeLandscapeCoreMLPipeline::videoTick(float) noexcept {}

auto MediaPipeLandscapeCoreMLPipeline::blurBackground(gs_texture_t *sourceTexture, gs_texture_t *alphaTexture,
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

void MediaPipeLandscapeCoreMLPipeline::videoRender(gs_effect_t *) noexcept
{
	// Skip rendering while the source or inference backend is unavailable.
	if (!obs_source_enabled(source_) || !isInitialized()) {
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
	if (!textureReader->read(modelInputTexture)) {
		blog(LOG_ERROR, OBS_LOG_HEADER "Model-input readback failed");
		os_log_with_type(osLogger(), OS_LOG_TYPE_ERROR, "Model-input readback failed");
		hasLastImage_ = false;
		obs_source_skip_video_filter(source_);
		return;
	}

	// Gate inference using the similarity of the current and previous model inputs.
	const bool maskSettingsChanged = maskSettingsDirty_.exchange(false, std::memory_order_relaxed);
	if (maskSettingsChanged || shouldInferMask(textureReader, imageSimilarityThreshold)) {
		// Infer a new model-resolution mask.
		if (!inferMask(textureReader)) {
			obs_source_skip_video_filter(source_);
			return;
		}

		// Refine the inferred mask into the output-resolution background mask.
		if (!refineMask(refineMaskEnabled, refineMaskThreshold, refineMaskContour, refineMaskExpansion)) {
			obs_source_skip_video_filter(source_);
			return;
		}
	}

	// Upload the CPU background mask to the GPU alpha texture.
	gs_texture_set_image(alphaTexture, backgroundMask_.get(), width_, false);
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
