// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <atomic>
#include <memory>

#include <ObsBridgeUtils/GsUnique.hpp>
#include <ObsBridgeUtils/TextureReader.hpp>
#include <os/log.h>

#include "BackgroundRemoval/AlignedBuffer.hpp"
#include "BackgroundRemoval/Operators.hpp"
#include "BackgroundRemoval/IRenderingPipeline.hpp"

namespace BackgroundRemoval {

class MainEffect;

class MediaPipeLandscapeCoreMLPipeline final : public IRenderingPipeline {
public:
	static constexpr std::int32_t kPipelineId = 56266;

	MediaPipeLandscapeCoreMLPipeline(const FilterProperty &property, obs_source_t *source,
					 MainEffect &backgroundRemovalEffect, std::uint32_t width,
					 std::uint32_t height);
	~MediaPipeLandscapeCoreMLPipeline() noexcept override;

	MediaPipeLandscapeCoreMLPipeline(const MediaPipeLandscapeCoreMLPipeline &) = delete;
	auto operator=(const MediaPipeLandscapeCoreMLPipeline &) -> MediaPipeLandscapeCoreMLPipeline & = delete;
	MediaPipeLandscapeCoreMLPipeline(MediaPipeLandscapeCoreMLPipeline &&) = delete;
	auto operator=(MediaPipeLandscapeCoreMLPipeline &&) -> MediaPipeLandscapeCoreMLPipeline & = delete;

	void update(const FilterProperty &property) noexcept override;
	void videoTick(float seconds) noexcept override;
	void videoRender(gs_effect_t *effect) noexcept override;
	auto pipelineId() const noexcept -> std::int32_t override { return kPipelineId; }
	auto getWidth() const noexcept -> std::uint32_t override { return width_; }
	auto getHeight() const noexcept -> std::uint32_t override { return height_; }

	std::atomic<bool> blurBackgroundEnabled_{};
	std::atomic<std::int64_t> blurBackgroundFactor_{};
	std::atomic<float> blurFocusPoint_{};
	std::atomic<float> blurFocusDepth_{};
	std::atomic<float> imageSimilarityThreshold_{};
	std::atomic<bool> refineMaskEnabled_{};
	std::atomic<float> refineMaskThreshold_{};
	std::atomic<float> refineMaskContour_{};
	std::atomic<std::int64_t> refineMaskExpansion_{};

private:
	static auto osLogger() noexcept -> os_log_t;
	static constexpr std::size_t maskWidth = 256;
	static constexpr std::size_t maskHeight = 144;
	static constexpr std::size_t maskPixelCount = maskWidth * maskHeight;
	static constexpr std::size_t modelInputHeight = maskHeight * 3;
	static constexpr std::size_t modelInputElementCount = maskPixelCount * 3;

	void initializePipeline();
	auto blurBackground(gs_texture_t *sourceTexture, gs_texture_t *alphaTexture,
			    const std::array<gs_texture_t *, 2> &blurTextures, std::size_t iterations, float focusPoint,
			    float focusDepth) noexcept -> gs_texture_t *;
	auto shouldInferMask(const ObsBridgeUtils::TextureReader *textureReader,
			     float similarityThreshold) const noexcept -> bool;
	auto inferMask(const ObsBridgeUtils::TextureReader *textureReader) noexcept -> bool;
	auto refineMask(bool refineMaskEnabled, float refineMaskThreshold, float refineMaskContour,
			std::int64_t refineMaskExpansion) noexcept -> bool;
	auto refinementFailed(const char *message) noexcept -> bool;
	auto processImage(const std::uint8_t *modelInput, float *outputBuffer) noexcept -> bool;
	auto isInitialized() const noexcept -> bool;

	const std::uint32_t width_;
	const std::uint32_t height_;
	obs_source_t *const source_;
	MainEffect &backgroundRemovalEffect_;
	ObsBridgeUtils::UniqueTexturePtr sourceTexture_;
	ObsBridgeUtils::UniqueTexturePtr modelInputTexture_;
	std::array<ObsBridgeUtils::UniqueTexturePtr, 2> blurTextures_;
	ObsBridgeUtils::UniqueTexturePtr alphaTexture_;
	Memory::AlignedBufferPtr backgroundMask_;
	Memory::AlignedBufferPtr outputMask_;
	Memory::AlignedBufferPtr inferenceOutput_;
	Memory::AlignedBufferPtr inferenceMask_;
	Memory::AlignedBufferPtr componentMask_;
	Memory::AlignedBufferPtr smoothingMask_;
	Operators::ResizeU8 resize_;
	Operators::SmoothU8 smooth_;
	Operators::ThresholdF32 modelThreshold_;
	Operators::ThresholdU8 outputThreshold_;
	Operators::MorphologyU8 outputMorphology_;
	Operators::RemoveSmallComponentsU8 removeSmallComponents_;
	bool hasLastImage_ = false;
	std::atomic<bool> maskSettingsDirty_{true};
	std::unique_ptr<ObsBridgeUtils::TextureReader> textureReader_;

	class CoreMLState;
	std::unique_ptr<CoreMLState> coreMLState_;
};

} // namespace BackgroundRemoval
