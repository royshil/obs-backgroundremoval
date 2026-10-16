// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "BackgroundRemoval/IRenderingPipeline.hpp"
#include "MaskRefinerDirect3D.hpp"
#include "MediaPipeLandscapeDifferenceHistory.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <tuple>

namespace BackgroundRemoval {

class MediaPipeLandscapeDirectMLEngine;

class MediaPipeLandscapeDirectMLPipeline : public IRenderingPipeline {
public:
	static constexpr std::int32_t kPipelineId = 62828;

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

	std::atomic<bool> blurBackgroundEnabled_{};
	std::atomic<std::int64_t> blurBackgroundFactor_{};
	std::atomic<float> blurFocusPoint_{};
	std::atomic<float> blurFocusDepth_{};
	std::atomic<float> imageSimilarityThreshold_{};
	std::atomic<bool> refineMaskEnabled_{};
	std::atomic<float> refineMaskThreshold_{};
	std::atomic<std::int64_t> refineMaskExpansion_{};

	static constexpr std::uint32_t kModelWidth = 256;
	static constexpr std::uint32_t kModelHeight = 144;
	static constexpr std::size_t kModelPixelCount = static_cast<std::size_t>(kModelWidth) * kModelHeight;

private:
	enum class FilterState { NeedsProcessing, Processing, ReadyForRendering };

	std::unique_ptr<MediaPipeLandscapeDirectMLEngine> engine_;
	std::unique_ptr<MaskRefinerDirect3D> maskRefiner_;
	const std::uint32_t width_;
	const std::uint32_t height_;
	obs_source_t *const source_;
	MediaPipeLandscapeDifferenceHistory differenceHistory_;
	FilterState filterState_ = FilterState::NeedsProcessing;
	bool frameBlurBackgroundEnabled_ = false;
	gs_texture_t *frameBlurredTexture_ = nullptr;
	void renderFrameResult() const noexcept;

	// MARK: - Textures and GPU Resources

	std::unique_ptr<gs_effect_t, void (*)(gs_effect_t *)> effect_;

	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> sourceTexture_;

	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> lastInputTexture_;
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> inputTexture_;

	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> lumaAbsDiffTexture_;
	std::unique_ptr<gs_stagesurf_t, void (*)(gs_stagesurf_t *)> lumaAbsDiffStagesurf_;

	std::array<std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)>, 2> blurTextures_;
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> alphaTexture_;
	std::optional<std::tuple<bool, float, std::int64_t>> appliedMaskSettings_;
};

} // namespace BackgroundRemoval
