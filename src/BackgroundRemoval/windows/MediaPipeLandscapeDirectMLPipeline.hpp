// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "BackgroundRemoval/IRenderingPipeline.hpp"
#include "BackgroundRemoval/Operators.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "DmlProgram/BaselineDmlProgram.hpp"

#include <ObsBridgeUtils/GsUnique.hpp>
#include <ObsBridgeUtils/TextureReader.hpp>

namespace BackgroundRemoval {

class MainEffect;

class MediaPipeLandscapeDirectMLPipeline : public IRenderingPipeline {
public:
	static constexpr std::int32_t kPipelineId = 62828;

	MediaPipeLandscapeDirectMLPipeline(const FilterProperty &property, obs_source_t *source,
					   MainEffect &backgroundRemovalEffect, std::uint32_t width,
					   std::uint32_t height);
	~MediaPipeLandscapeDirectMLPipeline() noexcept override;

	MediaPipeLandscapeDirectMLPipeline(const MediaPipeLandscapeDirectMLPipeline &) = delete;
	MediaPipeLandscapeDirectMLPipeline(MediaPipeLandscapeDirectMLPipeline &&) = delete;
	auto operator=(const MediaPipeLandscapeDirectMLPipeline &) -> MediaPipeLandscapeDirectMLPipeline & = delete;
	auto operator=(MediaPipeLandscapeDirectMLPipeline &&) -> MediaPipeLandscapeDirectMLPipeline & = delete;

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
	auto blurBackground(gs_texture_t *sourceTexture, gs_texture_t *alphaTexture,
			    const std::array<gs_texture_t *, 2> &blurTextures, std::size_t iterations, float focusPoint,
			    float focusDepth) noexcept -> gs_texture_t *;
	auto shouldInferMask(const ObsBridgeUtils::TextureReader *textureReader,
			     float similarityThreshold) const noexcept -> bool;
	auto inferMask(const ObsBridgeUtils::TextureReader *textureReader) noexcept -> bool;
	auto refineMask(bool refineMaskEnabled, float refineMaskThreshold, float refineMaskContour,
			std::int64_t refineMaskExpansion) noexcept -> bool;
	auto refinementFailed(const char *message) noexcept -> bool;
	auto processImage(const std::uint8_t *input, std::uint32_t linesize, std::uint8_t *output) noexcept -> bool;

	static constexpr std::uint32_t modelWidth = 256;
	static constexpr std::uint32_t modelHeight = 144;
	static constexpr std::size_t modelPixelCount = static_cast<std::size_t>(modelWidth) * modelHeight;

	std::unique_ptr<BaselineDml::BaselineDmlProgram> program_;
	const std::uint32_t width_;
	const std::uint32_t height_;
	obs_source_t *const source_;
	MainEffect &backgroundRemovalEffect_;
	ObsBridgeUtils::UniqueTexturePtr sourceTexture_;
	std::array<ObsBridgeUtils::UniqueTexturePtr, 2> blurTextures_;
	ObsBridgeUtils::UniqueTexturePtr modelInputTexture_;
	ObsBridgeUtils::UniqueTexturePtr alphaTexture_;
	std::unique_ptr<ObsBridgeUtils::TextureReader> textureReader_;
	std::vector<std::uint8_t> backgroundMask_;
	std::vector<std::uint8_t> modelMask_;
	std::vector<std::uint8_t> smoothedMask_;
	std::vector<float> lastInput_;
	std::vector<float> modelInput_;
	std::vector<float> modelOutput_;
	bool hasLastInput_ = false;
	std::atomic<bool> maskSettingsDirty_{true};
	Operators::PsnrF32 psnr_;
	Operators::ResizeU8 resize_;
	Operators::SmoothU8 smooth_;
	Operators::ThresholdU8 modelThreshold_;
	Operators::ThresholdU8 outputThreshold_;
	Operators::MorphologyU8 outputMorphology_;
	Operators::RemoveSmallComponentsU8 removeSmallComponents_;
};

} // namespace BackgroundRemoval
