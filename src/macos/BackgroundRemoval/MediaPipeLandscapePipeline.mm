// SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
// SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MediaPipeLandscapePipeline.hpp"
#include "../ImageProcessing.hpp"
#include "MainEffect.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <stdexcept>

#include <ObsBridgeUtils/AsyncTextureReader.hpp>
#include <ObsBridgeUtils/GsUnique.hpp>

#include "Memory/AlignedBuffer.hpp"

#import <CoreML/CoreML.h>
#import <Foundation/Foundation.h>

namespace BackgroundRemoval {

auto MediaPipeLandscapePipeline::osLogger() noexcept -> os_log_t
{
	static os_log_t logger = os_log_create("com.royshil.obs-backgroundremoval", "MediaPipeLandscapePipeline");
	return logger;
}

class MediaPipeLandscapePipeline::CoreMLState final {
public:
	MLModel *model = nil;
	MLMultiArray *input = nil;
	MLMultiArray *output = nil;
	MLDictionaryFeatureProvider *features = nil;
	MLPredictionOptions *predictionOptions = nil;
};

namespace {

auto createMorphologyTemporaryBuffer(std::size_t width, std::size_t height) -> Memory::AlignedBufferPtr
{
	vImage_Buffer source{nullptr, height, width, width};
	vImage_Buffer destination{nullptr, height, width, width};
	const vImage_Error dilationSize =
		vImageMax_Planar8(&source, &destination, nullptr, 0, 0, 3, 3, kvImageGetTempBufferSize);
	if (dilationSize < kvImageNoError) {
		throw std::runtime_error("Failed to query vImage dilation temporary buffer size");
	}
	const vImage_Error erosionSize =
		vImageMin_Planar8(&source, &destination, nullptr, 0, 0, 3, 3, kvImageGetTempBufferSize);
	if (erosionSize < kvImageNoError) {
		throw std::runtime_error("Failed to query vImage erosion temporary buffer size");
	}
	return Memory::makeAlignedBytes(static_cast<std::size_t>(std::max(dilationSize, erosionSize)));
}

auto createScaleTemporaryBuffer(std::size_t destinationWidth, std::size_t destinationHeight) -> Memory::AlignedBufferPtr
{
	vImage_Buffer source{nullptr, 144, 256, 256};
	vImage_Buffer destination{nullptr, destinationHeight, destinationWidth, destinationWidth};
	const vImage_Error temporaryBufferSize = vImageScale_Planar8(
		&source, &destination, nullptr, kvImageGetTempBufferSize | kvImageHighQualityResampling);
	if (temporaryBufferSize < kvImageNoError) {
		throw std::runtime_error("Failed to query vImage scale temporary buffer size");
	}
	return Memory::makeAlignedBytes(static_cast<std::size_t>(temporaryBufferSize));
}

auto createSmoothingTemporaryBuffer(std::size_t width, std::size_t height) -> Memory::AlignedBufferPtr
{
	vImage_Buffer source{nullptr, height, width, width};
	vImage_Buffer destination{nullptr, height, width, width};
	const vImage_Error temporaryBufferSize = vImageTentConvolve_Planar8(
		&source, &destination, nullptr, 0, 0, 9, 9, 0, kvImageGetTempBufferSize | kvImageEdgeExtend);
	if (temporaryBufferSize < kvImageNoError) {
		throw std::runtime_error("Failed to query vImage smoothing temporary buffer size");
	}
	return Memory::makeAlignedBytes(static_cast<std::size_t>(temporaryBufferSize));
}

} // namespace

void MediaPipeLandscapePipeline::initializePipeline()
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

auto MediaPipeLandscapePipeline::processImage(const std::uint8_t *modelInput, float similarityThreshold,
					      float *outputBuffer) -> bool
{
	@autoreleasepool {
		const auto *newInput = reinterpret_cast<const _Float16 *>(modelInput);
		auto *inputData = static_cast<_Float16 *>(coreMLState_->input.dataPointer);
		if (hasLastImage_) {
			double squaredError = 0.0;
			for (std::size_t index = 0; index < modelInputElementCount; ++index) {
				const double difference = static_cast<double>(newInput[index]) - inputData[index];
				squaredError += difference * difference;
			}
			const double meanSquaredError = squaredError / static_cast<double>(modelInputElementCount);
			const double psnr = meanSquaredError == 0.0 ? std::numeric_limits<double>::infinity()
								    : 10.0 * std::log10(1.0 / meanSquaredError);
			if (psnr > similarityThreshold) {
				return false;
			}
		}
		std::memcpy(inputData, newInput, modelInputElementCount * sizeof(_Float16));
		hasLastImage_ = true;

		NSError *error = nil;
		id<MLFeatureProvider> prediction = [coreMLState_->model
			predictionFromFeatures:coreMLState_->features
				       options:coreMLState_->predictionOptions
					 error:&error];
		if (!prediction) {
			throw std::runtime_error(error.localizedDescription.UTF8String ?: "Core ML inference failed");
		}
		const auto *outputData = static_cast<const _Float16 *>(coreMLState_->output.dataPointer);
		for (std::size_t index = 0; index < maskPixelCount; ++index) {
			outputBuffer[index] = 1.0F - static_cast<float>(outputData[index]);
		}
		return true;
	}
}

auto MediaPipeLandscapePipeline::isInitialized() const noexcept -> bool
{
	return coreMLState_->model != nil;
}

MediaPipeLandscapePipeline::MediaPipeLandscapePipeline(obs_data_t *settings, obs_source_t *source, std::uint32_t width,
						       std::uint32_t height)
	: width_(width),
	  height_(height),
	  source_(source),
	  settingsData_(obs_data_create(), obs_data_release),
	  backgroundMask_(Memory::makeAlignedBytes(static_cast<std::size_t>(width) * height)),
	  outputMask_(Memory::makeAlignedBytes(static_cast<std::size_t>(width) * height)),
	  outputMorphologyMask_(Memory::makeAlignedBytes(static_cast<std::size_t>(width) * height)),
	  inferenceOutput_(Memory::makeAlignedBytes(maskPixelCount * sizeof(float))),
	  inferenceMask_(Memory::makeAlignedBytes(maskPixelCount)),
	  componentMask_(Memory::makeAlignedBytes(maskPixelCount)),
	  morphologyMask_(Memory::makeAlignedBytes(maskPixelCount)),
	  smoothingMask_(Memory::makeAlignedBytes(maskPixelCount)),
	  morphologyTemporaryBuffer_(createMorphologyTemporaryBuffer(maskWidth, maskHeight)),
	  smoothingTemporaryBuffer_(createSmoothingTemporaryBuffer(maskWidth, maskHeight)),
	  outputMorphologyTemporaryBuffer_(createMorphologyTemporaryBuffer(width, height)),
	  scaleTemporaryBuffer_(createScaleTemporaryBuffer(width, height)),
	  coreMLState_(std::make_unique<CoreMLState>())
{
	using namespace ObsBridgeUtils;

	if (!settings) {
		throw std::invalid_argument("Background removal settings are null");
	} else if (!source) {
		throw std::invalid_argument("Background removal source is null");
	} else if (width == 0) {
		throw std::invalid_argument("Background removal width is zero");
	} else if (height == 0) {
		throw std::invalid_argument("Background removal height is zero");
	} else if (!settingsData_) {
		throw std::invalid_argument("Background removal settings snapshot is null");
	}
	std::fill_n(backgroundMask_.get(), static_cast<std::size_t>(width) * height, std::uint8_t{255});
	initializePipeline();

	obs_enter_graphics();
	try {
		textureReader_ = std::make_unique<AsyncTextureReader>(maskWidth, modelInputHeight, GS_R16F);
		sourceTextures_[0] = makeUniqueTexture(width, height, GS_BGRA, GS_RENDER_TARGET);
		sourceTextures_[1] = makeUniqueTexture(width, height, GS_BGRA, GS_RENDER_TARGET);
		modelInputTexture_ = makeUniqueTexture(maskWidth, modelInputHeight, GS_R16F, GS_RENDER_TARGET);
		blurTextures_[0] = makeUniqueTexture(width, height, GS_BGRA, GS_RENDER_TARGET);
		blurTextures_[1] = makeUniqueTexture(width, height, GS_BGRA, GS_RENDER_TARGET);
		alphaTexture_ = makeUniqueTexture(width, height, GS_R8, GS_DYNAMIC);
		mainEffect_ = std::make_unique<Detail::MainEffect>();
		if (!sourceTextures_[0] || !sourceTextures_[1] || !modelInputTexture_ || !blurTextures_[0] ||
		    !blurTextures_[1] || !alphaTexture_) {
			throw std::runtime_error("Failed to create rendering textures");
		}
	} catch (...) {
		mainEffect_.reset();
		sourceTextures_[0].reset();
		sourceTextures_[1].reset();
		modelInputTexture_.reset();
		blurTextures_[0].reset();
		blurTextures_[1].reset();
		alphaTexture_.reset();
		textureReader_.reset();
		GsUnique::drain();
		obs_leave_graphics();
		throw;
	}
	obs_leave_graphics();

	update(settings);
	blog(LOG_INFO, OBS_LOG_HEADER "MediaPipe Landscape rendering pipeline initialized (%ux%u)", width_, height_);
	os_log_with_type(osLogger(), OS_LOG_TYPE_INFO,
			 "MediaPipe Landscape rendering pipeline initialized (%{public}ux%{public}u)", width_, height_);
}

MediaPipeLandscapePipeline::~MediaPipeLandscapePipeline() noexcept
{
	using namespace ObsBridgeUtils;

	obs_enter_graphics();
	mainEffect_.reset();
	sourceTextures_[0].reset();
	sourceTextures_[1].reset();
	modelInputTexture_.reset();
	blurTextures_[0].reset();
	blurTextures_[1].reset();
	alphaTexture_.reset();
	textureReader_.reset();
	GsUnique::drain();
	obs_leave_graphics();
}

void MediaPipeLandscapePipeline::update(obs_data_t *settings) noexcept
{
	if (!settings || !settingsData_) {
		blog(LOG_ERROR, OBS_LOG_HEADER "Current or new settings are null");
		os_log_with_type(osLogger(), OS_LOG_TYPE_ERROR, "Current or new settings are null");
		return;
	}

	obs_data_t *oldSettings = settingsData_.get();
	obs_data_t *newSettings = settings;

	const bool oldBlurBackgroundEnabled = obs_data_get_bool(oldSettings, "v2_BlurBg");
	const bool newBlurBackgroundEnabled = obs_data_get_bool(newSettings, "v2_BlurBg");
	if (newBlurBackgroundEnabled != oldBlurBackgroundEnabled) {
		enableBlurBackground.store(newBlurBackgroundEnabled, std::memory_order_relaxed);
	}

	const std::int64_t oldBlurBackground = obs_data_get_int(oldSettings, "v2_BlurBg_Factor");
	const std::int64_t newBlurBackground = obs_data_get_int(newSettings, "v2_BlurBg_Factor");
	if (newBlurBackground != oldBlurBackground) {
		blurBackgroundFactor.store(newBlurBackground, std::memory_order_relaxed);
	}

	const double oldBlurFocusPoint = obs_data_get_double(oldSettings, "v2_BlurBg_FocusPoint");
	const double newBlurFocusPoint = obs_data_get_double(newSettings, "v2_BlurBg_FocusPoint");
	if (newBlurFocusPoint != oldBlurFocusPoint) {
		blurFocusPoint.store(static_cast<float>(newBlurFocusPoint), std::memory_order_relaxed);
	}

	const double oldBlurFocusDepth = obs_data_get_double(oldSettings, "v2_BlurBg_FocusDepth");
	const double newBlurFocusDepth = obs_data_get_double(newSettings, "v2_BlurBg_FocusDepth");
	if (newBlurFocusDepth != oldBlurFocusDepth) {
		blurFocusDepth.store(static_cast<float>(newBlurFocusDepth), std::memory_order_relaxed);
	}

	const double oldImageSimilarityThreshold = obs_data_get_double(oldSettings, "v2_Model_MatteSensitivity");
	const double newImageSimilarityThreshold = obs_data_get_double(newSettings, "v2_Model_MatteSensitivity");
	if (newImageSimilarityThreshold != oldImageSimilarityThreshold) {
		imageSimilarityThreshold.store(static_cast<float>(newImageSimilarityThreshold),
					       std::memory_order_relaxed);
	}

	const bool oldThresholdEnabled = obs_data_get_bool(oldSettings, "v2_RefineMask");
	const bool newThresholdEnabled = obs_data_get_bool(newSettings, "v2_RefineMask");
	if (newThresholdEnabled != oldThresholdEnabled) {
		enableThreshold.store(newThresholdEnabled, std::memory_order_relaxed);
	}

	const double oldThreshold = obs_data_get_double(oldSettings, "v2_RefineMask_Threshold");
	const double newThreshold = obs_data_get_double(newSettings, "v2_RefineMask_Threshold");
	if (newThreshold != oldThreshold) {
		threshold.store(static_cast<float>(newThreshold), std::memory_order_relaxed);
	}

	const double oldContourFilter = obs_data_get_double(oldSettings, "v2_RefineMask_Contour");
	const double newContourFilter = obs_data_get_double(newSettings, "v2_RefineMask_Contour");
	if (newContourFilter != oldContourFilter) {
		contourFilter.store(static_cast<float>(newContourFilter), std::memory_order_relaxed);
	}

	const std::int64_t oldMaskExpansion = obs_data_get_int(oldSettings, "v2_RefineMask_Expansion");
	const std::int64_t newMaskExpansion = obs_data_get_int(newSettings, "v2_RefineMask_Expansion");
	if (newMaskExpansion != oldMaskExpansion) {
		maskExpansion.store(newMaskExpansion, std::memory_order_relaxed);
	}

	obs_data_clear(settingsData_.get());
	obs_data_apply(settingsData_.get(), settings);
}

auto MediaPipeLandscapePipeline::processStagedFrame() -> StagedFrameResult
{
	try {
		if (!textureReader_->sync()) {
			return StagedFrameResult::NotReady;
		}
	} catch (const std::exception &exception) {
		blog(LOG_ERROR, OBS_LOG_HEADER "Failed to synchronize source texture: %s", exception.what());
		os_log_with_type(osLogger(), OS_LOG_TYPE_ERROR, "Failed to synchronize source texture: %{public}s",
				 exception.what());
		hasLastImage_ = false;
		return StagedFrameResult::Failed;
	}
	try {
		const auto &modelInput = textureReader_->getBuffer();
		auto *inferenceOutputData = reinterpret_cast<float *>(inferenceOutput_.get());
		if (!processImage(modelInput.data(), imageSimilarityThreshold.load(std::memory_order_relaxed),
				  inferenceOutputData)) {
			return StagedFrameResult::Completed;
		}
		const bool thresholdEnabled = enableThreshold.load(std::memory_order_relaxed);
		if (thresholdEnabled) {
			const float thresholdValue =
				1.0F - std::clamp(threshold.load(std::memory_order_relaxed), 0.0F, 1.0F);
			ImageProcessing::thresholdBinaryMask(inferenceOutputData, inferenceMask_.get(), maskPixelCount,
							     thresholdValue);
		} else {
			const vImage_Buffer inferenceOutputBuffer{inferenceOutputData, maskHeight, maskWidth,
								  maskWidth * sizeof(float)};
			const vImage_Buffer inferenceMaskBuffer{inferenceMask_.get(), maskHeight, maskWidth, maskWidth};
			ImageProcessing::convertFloatMask(inferenceOutputBuffer, inferenceMaskBuffer);
		}

		if (thresholdEnabled) {
			const float contourFilterValue = contourFilter.load(std::memory_order_relaxed);
			if (contourFilterValue > 0.0F && contourFilterValue < 1.0F) {
				connectedComponentFilter_.filter(inferenceMask_.get(), componentMask_.get(),
								 static_cast<double>(maskPixelCount) *
									 contourFilterValue);
				inferenceMask_.swap(componentMask_);
			}
			const vImage_Buffer inferenceMaskBuffer{inferenceMask_.get(), maskHeight, maskWidth, maskWidth};
			const vImage_Buffer morphologyMaskBuffer{morphologyMask_.get(), maskHeight, maskWidth,
								 maskWidth};
			const vImage_Buffer componentMaskBuffer{componentMask_.get(), maskHeight, maskWidth, maskWidth};
			if (vImageMax_Planar8(&inferenceMaskBuffer, &morphologyMaskBuffer,
					      morphologyTemporaryBuffer_.get(), 0, 0, 3, 3,
					      kvImageNoFlags) != kvImageNoError) {
				throw std::runtime_error("vImage binary-mask dilation failed");
			}
			if (vImageMin_Planar8(&morphologyMaskBuffer, &componentMaskBuffer,
					      morphologyTemporaryBuffer_.get(), 0, 0, 3, 3,
					      kvImageNoFlags) != kvImageNoError) {
				throw std::runtime_error("vImage binary-mask erosion failed");
			}
			inferenceMask_.swap(componentMask_);
		}

		const vImage_Buffer inferenceMaskBuffer{inferenceMask_.get(), maskHeight, maskWidth, maskWidth};
		const vImage_Buffer outputMaskBuffer{outputMask_.get(), height_, width_, width_};
		if (thresholdEnabled) {
			const vImage_Buffer smoothingMaskBuffer{smoothingMask_.get(), maskHeight, maskWidth, maskWidth};
			if (vImageTentConvolve_Planar8(&inferenceMaskBuffer, &smoothingMaskBuffer,
						       smoothingTemporaryBuffer_.get(), 0, 0, 9, 9, 0,
						       kvImageEdgeExtend) != kvImageNoError) {
				throw std::runtime_error("vImage mask smoothing failed");
			}
			if (vImageScale_Planar8(&smoothingMaskBuffer, &outputMaskBuffer, scaleTemporaryBuffer_.get(),
						kvImageHighQualityResampling) != kvImageNoError) {
				throw std::runtime_error("vImage smoothed-mask scaling failed");
			}
			ImageProcessing::thresholdBinaryMask(
				outputMask_.get(), static_cast<std::size_t>(width_) * height_, std::uint8_t{128});
		} else if (vImageScale_Planar8(&inferenceMaskBuffer, &outputMaskBuffer, scaleTemporaryBuffer_.get(),
					       kvImageHighQualityResampling) != kvImageNoError) {
			throw std::runtime_error("vImage mask scaling failed");
		}

		if (thresholdEnabled) {
			const std::int64_t maskExpansionValue = maskExpansion.load(std::memory_order_relaxed);
			if (maskExpansionValue > 0) {
				vImage_Buffer sourceBuffer{outputMask_.get(), height_, width_, width_};
				vImage_Buffer destinationBuffer{outputMorphologyMask_.get(), height_, width_, width_};
				for (std::int64_t iteration = 0; iteration < maskExpansionValue; ++iteration) {
					if (vImageMin_Planar8(&sourceBuffer, &destinationBuffer,
							      outputMorphologyTemporaryBuffer_.get(), 0, 0, 3, 3,
							      kvImageNoFlags) != kvImageNoError) {
						throw std::runtime_error("vImage output-mask erosion failed");
					}
					std::swap(sourceBuffer.data, destinationBuffer.data);
				}
				if (sourceBuffer.data != outputMask_.get()) {
					outputMask_.swap(outputMorphologyMask_);
				}
			} else if (maskExpansionValue < 0) {
				vImage_Buffer sourceBuffer{outputMask_.get(), height_, width_, width_};
				vImage_Buffer destinationBuffer{outputMorphologyMask_.get(), height_, width_, width_};
				for (std::int64_t iteration = 0; iteration < -maskExpansionValue; ++iteration) {
					if (vImageMax_Planar8(&sourceBuffer, &destinationBuffer,
							      outputMorphologyTemporaryBuffer_.get(), 0, 0, 3, 3,
							      kvImageNoFlags) != kvImageNoError) {
						throw std::runtime_error("vImage output-mask dilation failed");
					}
					std::swap(sourceBuffer.data, destinationBuffer.data);
				}
				if (sourceBuffer.data != outputMask_.get()) {
					outputMask_.swap(outputMorphologyMask_);
				}
			}
		}
		backgroundMask_.swap(outputMask_);
		return StagedFrameResult::Completed;
	} catch (const std::exception &exception) {
		blog(LOG_ERROR, OBS_LOG_HEADER "Background-removal inference failed: %s", exception.what());
		os_log_with_type(osLogger(), OS_LOG_TYPE_ERROR, "Background-removal inference failed: %{public}s",
				 exception.what());
		hasLastImage_ = false;
		return StagedFrameResult::Failed;
	}
}

void MediaPipeLandscapePipeline::videoTick(float) {}

void MediaPipeLandscapePipeline::videoRender(gs_effect_t *)
{
	if (!obs_source_enabled(source_) || !mainEffect_ || !isInitialized()) {
		obs_source_skip_video_filter(source_);
		return;
	}

	bool canCapture = true;
	if (hasStagedTexture_) {
		const StagedFrameResult result = processStagedFrame();
		if (result == StagedFrameResult::Completed) {
			completedSourceIndex_ = stagedSourceIndex_;
			hasCompletedFrame_ = true;
			hasStagedTexture_ = false;
		} else if (result == StagedFrameResult::Failed) {
			hasStagedTexture_ = false;
		} else {
			canCapture = false;
		}
	}

	const std::size_t captureIndex = nextCaptureIndex_;
	if (canCapture) {
		obs_source_t *target = obs_filter_get_target(source_);
		if (!target) {
			obs_source_skip_video_filter(source_);
			return;
		}
		gs_texture_t *const captureTexture = sourceTextures_[captureIndex].get();
		mainEffect_->drawSource(captureTexture, target);
		mainEffect_->packModelInput(modelInputTexture_.get(), captureTexture);
		if (textureReader_->stage(modelInputTexture_.get())) {
			stagedSourceIndex_ = captureIndex;
			hasStagedTexture_ = true;
			nextCaptureIndex_ = 1 - captureIndex;
		}
	} else if (!hasCompletedFrame_) {
		obs_source_skip_video_filter(source_);
		return;
	}

	const std::size_t renderIndex = hasCompletedFrame_ ? completedSourceIndex_ : captureIndex;
	gs_texture_t *const renderTexture = sourceTextures_[renderIndex].get();
	gs_texture_set_image(alphaTexture_.get(), backgroundMask_.get(), width_, false);

	const bool blurBackgroundEnabled = enableBlurBackground.load(std::memory_order_relaxed);
	const std::int64_t blurIterations = blurBackgroundFactor.load(std::memory_order_relaxed);
	gs_texture_t *blurredTexture = renderTexture;
	if (blurBackgroundEnabled) {
		gs_texture_t *blurSourceTexture = renderTexture;
		for (int iteration = 0; iteration < blurIterations; ++iteration) {
			gs_texture_t *blurTargetTexture = blurTextures_[iteration % 2].get();
			mainEffect_->focalBlur(blurTargetTexture, blurSourceTexture, alphaTexture_.get(), iteration,
					       blurIterations, blurFocusPoint.load(std::memory_order_relaxed),
					       blurFocusDepth.load(std::memory_order_relaxed));
			blurSourceTexture = blurTargetTexture;
		}
		blurredTexture = blurSourceTexture;
	}

	gs_blend_state_push();
	gs_reset_blend_state();
	if (blurBackgroundEnabled) {
		mainEffect_->directDrawWithBlurredBackground(renderTexture, alphaTexture_.get(), blurredTexture);
	} else {
		mainEffect_->directDrawWithoutBlur(renderTexture, alphaTexture_.get());
	}
	gs_blend_state_pop();
}

auto MediaPipeLandscapePipeline::getWidth() const noexcept -> std::uint32_t
{
	return width_;
}

auto MediaPipeLandscapePipeline::getHeight() const noexcept -> std::uint32_t
{
	return height_;
}

} // namespace BackgroundRemoval
