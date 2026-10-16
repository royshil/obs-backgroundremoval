// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "BackgroundRemoval/IRenderingPipeline.hpp"
#include "MediaPipeLandscapeDifferenceHistory.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <tuple>
#include <source_location>

namespace BackgroundRemoval {

class MediaPipeLandscapeDirectMLEngine;

class MediaPipeLandscapeDirectMLPipeline : public IRenderingPipeline {
public:
	static constexpr std::int32_t kPipelineId = 62828;

	class InitError : public std::exception {
	public:
		explicit InitError(std::source_location loc = std::source_location::current()) noexcept : loc_(loc) {}
		const char *what() const noexcept override { return "MPLDPipeline.InitError"; }
		const std::source_location &loc() const noexcept { return loc_; }

	private:
		const std::source_location loc_;
	};

	MediaPipeLandscapeDirectMLPipeline(const FilterProperty &property, obs_source_t *source, std::uint32_t width,
					   std::uint32_t height);
	~MediaPipeLandscapeDirectMLPipeline() noexcept override;

	MediaPipeLandscapeDirectMLPipeline(const MediaPipeLandscapeDirectMLPipeline &) = delete;
	MediaPipeLandscapeDirectMLPipeline(MediaPipeLandscapeDirectMLPipeline &&) = delete;
	auto operator=(const MediaPipeLandscapeDirectMLPipeline &) -> MediaPipeLandscapeDirectMLPipeline & = delete;
	auto operator=(MediaPipeLandscapeDirectMLPipeline &&) -> MediaPipeLandscapeDirectMLPipeline & = delete;

	auto autoSetSensitivity() noexcept -> bool;
	void update(const FilterProperty &property) noexcept override;
	void videoTick(float seconds) noexcept override;
	void videoRender(gs_effect_t *effect) noexcept override;
	auto pipelineId() const noexcept -> std::int32_t override { return kPipelineId; }
	auto getWidth() const noexcept -> std::uint32_t override { return width_; }
	auto getHeight() const noexcept -> std::uint32_t override { return height_; }

	auto needsRecreation() const noexcept -> bool;

	std::atomic<bool> blurBackgroundEnabled_{};
	std::atomic<std::int64_t> blurBackgroundFactor_{};
	std::atomic<float> blurFocusPoint_{};
	std::atomic<float> blurFocusDepth_{};
	std::atomic<float> imageSimilarityThreshold_{};
	std::atomic<bool> refineMaskEnabled_{};
	std::atomic<float> refineMaskThreshold_{};
	std::atomic<std::uint32_t> additionalInferenceFrames_{1};

	static constexpr std::uint32_t kModelWidth = 256;
	static constexpr std::uint32_t kModelHeight = 144;
	static constexpr double kModelInputMaximumSum = 9400320.0;

private:
	friend struct MediaPipeLandscapeDirectMLPipelineTestAccess;
	enum class FilterState { NeedsProcessing, Processing, ReadyForRendering };

	std::unique_ptr<MediaPipeLandscapeDirectMLEngine> engine_;
	const std::uint32_t width_;
	const std::uint32_t height_;
	obs_source_t *const source_;
	MediaPipeLandscapeDifferenceHistory differenceHistory_;
	FilterState filterState_ = FilterState::NeedsProcessing;
	bool frameBlurBackgroundEnabled_ = false;
	gs_texture_t *frameBlurredTexture_ = nullptr;
	bool needsRecreation_ = false;
	bool gateSubmissionPending_ = false;
	bool gateResultAvailable_ = false;
	std::uint32_t remainingInferenceFrames_ = 0;

	// MARK: - Textures and GPU Resources

	std::unique_ptr<gs_effect_t, void (*)(gs_effect_t *)> effect_;
	gs_eparam_t *const epImage_;
	gs_eparam_t *const epImage1_;
	gs_eparam_t *const epImage2_;

	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> targetTexture_;
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> inputTexture_;
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> lastInputTexture_;

	std::array<std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)>, 2> blurTextures_;
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> alphaTexture_;
	std::optional<std::tuple<bool, float>> appliedMaskSettings_;
};

} // namespace BackgroundRemoval
