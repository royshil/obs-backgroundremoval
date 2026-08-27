// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <atomic>
#include <memory>

#include <ObsBridgeUtils/AsyncTextureReader.hpp>
#include <ObsBridgeUtils/GsUnique.hpp>
#include <os/log.h>

#include "Memory/AlignedBuffer.hpp"
#include "../ImageProcessing.hpp"
#include "IRenderingPipeline.hpp"

namespace BackgroundRemoval {

namespace Detail {
class MainEffect;
}

class MediaPipeLandscapePipeline final : public IRenderingPipeline {
public:
	MediaPipeLandscapePipeline(obs_data_t *settings, obs_source_t *source, std::uint32_t width,
				   std::uint32_t height);
	~MediaPipeLandscapePipeline() noexcept override;

	MediaPipeLandscapePipeline(const MediaPipeLandscapePipeline &) = delete;
	auto operator=(const MediaPipeLandscapePipeline &) -> MediaPipeLandscapePipeline & = delete;
	MediaPipeLandscapePipeline(MediaPipeLandscapePipeline &&) = delete;
	auto operator=(MediaPipeLandscapePipeline &&) -> MediaPipeLandscapePipeline & = delete;

	void update(obs_data_t *settings) noexcept override;
	void videoTick(float seconds) override;
	void videoRender(gs_effect_t *effect) override;
	auto getWidth() const noexcept -> std::uint32_t override;
	auto getHeight() const noexcept -> std::uint32_t override;

	std::atomic<bool> enableBlurBackground{false};
	std::atomic<std::int64_t> blurBackgroundFactor{0};
	std::atomic<float> blurFocusPoint{0.1F};
	std::atomic<float> blurFocusDepth{0.0F};
	std::atomic<float> imageSimilarityThreshold{35.0F};
	std::atomic<bool> enableThreshold{true};
	std::atomic<float> threshold{0.5F};
	std::atomic<float> contourFilter{0.05F};
	std::atomic<std::int64_t> maskExpansion{0};

private:
	enum class StagedFrameResult {
		NotReady,
		Completed,
		Failed,
	};

	static auto osLogger() noexcept -> os_log_t;
	static constexpr std::size_t maskWidth = 256;
	static constexpr std::size_t maskHeight = 144;
	static constexpr std::size_t maskPixelCount = maskWidth * maskHeight;
	static constexpr std::size_t modelInputHeight = maskHeight * 3;
	static constexpr std::size_t modelInputElementCount = maskPixelCount * 3;

	void initializePipeline();
	auto processStagedFrame() -> StagedFrameResult;
	auto processImage(const std::uint8_t *modelInput, float similarityThreshold, float *outputBuffer) -> bool;
	auto isInitialized() const noexcept -> bool;

	const std::uint32_t width_;
	const std::uint32_t height_;
	obs_source_t *const source_;
	std::unique_ptr<obs_data_t, void (*)(obs_data_t *)> settingsData_;
	std::array<ObsBridgeUtils::UniqueTexturePtr, 2> sourceTextures_;
	ObsBridgeUtils::UniqueTexturePtr modelInputTexture_;
	std::array<ObsBridgeUtils::UniqueTexturePtr, 2> blurTextures_;
	ObsBridgeUtils::UniqueTexturePtr alphaTexture_;
	Memory::AlignedBufferPtr backgroundMask_;
	Memory::AlignedBufferPtr outputMask_;
	Memory::AlignedBufferPtr outputMorphologyMask_;
	Memory::AlignedBufferPtr inferenceOutput_;
	Memory::AlignedBufferPtr inferenceMask_;
	Memory::AlignedBufferPtr componentMask_;
	Memory::AlignedBufferPtr morphologyMask_;
	Memory::AlignedBufferPtr smoothingMask_;
	Memory::AlignedBufferPtr morphologyTemporaryBuffer_;
	Memory::AlignedBufferPtr smoothingTemporaryBuffer_;
	Memory::AlignedBufferPtr outputMorphologyTemporaryBuffer_;
	Memory::AlignedBufferPtr scaleTemporaryBuffer_;
	ImageProcessing::ConnectedComponentFilter<maskWidth, maskHeight, std::uint16_t> connectedComponentFilter_;
	bool hasLastImage_ = false;
	std::unique_ptr<Detail::MainEffect> mainEffect_;
	std::unique_ptr<ObsBridgeUtils::AsyncTextureReader> textureReader_;
	std::size_t nextCaptureIndex_ = 0;
	std::size_t stagedSourceIndex_ = 0;
	std::size_t completedSourceIndex_ = 0;
	bool hasStagedTexture_ = false;
	bool hasCompletedFrame_ = false;

	class CoreMLState;
	std::unique_ptr<CoreMLState> coreMLState_;
};

} // namespace BackgroundRemoval
