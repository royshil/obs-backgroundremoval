// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <exception>
#include <memory>
#include <source_location>

#include "IRenderingPipeline.hpp"

namespace BackgroundRemoval {

class MPLD10Engine;

class MPLD10Pipeline : public IRenderingPipeline {
public:
	class InitError : public std::exception {
	public:
		explicit InitError(std::source_location loc = std::source_location::current()) noexcept : loc_(loc) {}
		const char *what() const noexcept override { return "MPLD10Pipeline.InitError"; }
		const std::source_location &loc() const noexcept { return loc_; }

	private:
		const std::source_location loc_;
	};

	MPLD10Pipeline(obs_source_t *source, std::uint32_t width, std::uint32_t height);
	~MPLD10Pipeline() noexcept override;

	MPLD10Pipeline(const MPLD10Pipeline &) = delete;
	MPLD10Pipeline(MPLD10Pipeline &&) = delete;
	auto operator=(const MPLD10Pipeline &) -> MPLD10Pipeline & = delete;
	auto operator=(MPLD10Pipeline &&) -> MPLD10Pipeline & = delete;

	auto getMotion() const noexcept -> std::optional<double> override;
	void setMotionSuppression(double value) noexcept override;
	void setMaskAdjustment(double value) noexcept override;
	void setBlurBgEnabled(bool value) noexcept override;
	void setBlurBgFactor(std::int64_t value) noexcept override;
	void setBlurBgFocusPoint(double value) noexcept override;
	void setBlurBgFocusDepth(double value) noexcept override;
	void videoTick(float seconds) noexcept override;
	void videoRender(gs_effect_t *effect) noexcept override;
	auto pipelineId() const noexcept -> std::int32_t override { return kMPLD10PipelineId; }
	auto width() const noexcept -> std::uint32_t override { return width_; }
	auto height() const noexcept -> std::uint32_t override { return height_; }

	auto needsRecreation() const noexcept -> bool override;

	std::atomic<bool> blurBackgroundEnabled_{};
	std::atomic<std::int64_t> blurBackgroundFactor_{};
	std::atomic<float> blurFocusPoint_{};
	std::atomic<float> blurFocusDepth_{};
	std::atomic<float> imageSimilarityThreshold_{};
	std::atomic<float> refineMaskThreshold_{};
	std::atomic<std::uint32_t> additionalInferenceFrames_{1};

	static constexpr std::uint32_t kModelWidth = 256;
	static constexpr std::uint32_t kModelHeight = 144;
	static constexpr double kModelInputMaximumSum = 9400320.0;

private:
	friend struct MPLD10PipelineTestAccess;
	enum class FilterState { NeedsProcessing, Processing, ReadyForRendering };

	std::unique_ptr<MPLD10Engine> engine_;
	const std::uint32_t width_;
	const std::uint32_t height_;
	obs_source_t *const source_;
	std::atomic<double> motion_{-1.0};
	FilterState filterState_ = FilterState::NeedsProcessing;
	bool frameBlurBackgroundEnabled_ = false;
	gs_texture_t *frameBlurredTexture_ = nullptr;
	bool needsRecreation_ = false;
	std::uint32_t fpsNumerator_ = 0;
	std::uint32_t fpsDenominator_ = 0;
	void updateVideoTiming() noexcept;
	bool differenceStaged_ = false;
	std::uint32_t remainingInferenceFrames_ = 0;

	// MARK: - Textures and GPU Resources

	std::unique_ptr<gs_effect_t, void (*)(gs_effect_t *)> renderingEffect_;
	gs_eparam_t *const epImage_;
	gs_eparam_t *const epImage1_;
	gs_eparam_t *const epImage2_;
	std::unique_ptr<gs_effect_t, void (*)(gs_effect_t *)> gateEffect_;
	gs_eparam_t *const gateImage_;
	gs_eparam_t *const gateImage1_;

	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> targetTexture_;
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> inputTexture_;
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> lastInputTexture_;
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> lumaAbsDiffTexture_;
	std::unique_ptr<gs_stagesurf_t, void (*)(gs_stagesurf_t *)> lumaAbsDiffStagesurf_;

	std::array<std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)>, 2> blurTextures_;
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> alphaTexture_;
};

} // namespace BackgroundRemoval
