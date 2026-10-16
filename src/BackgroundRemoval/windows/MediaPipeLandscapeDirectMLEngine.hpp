// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <cstdint>
#include <memory>
#include <limits>
#include <span>

struct ID3D11Texture2D;
struct ID3D12Device;
struct ID3D12CommandQueue;
struct ID3D12Resource;

struct gs_texture;
typedef struct gs_texture gs_texture_t;

namespace BackgroundRemoval {

class MediaPipeLandscapeDirectMLEngine final {
public:
	using HRESULT = long;

	enum class Output { CpuProbabilities, GpuMask };
	struct GateResult {
		std::uint32_t sum = 0;
		std::uint32_t motionCount = 0;
		bool motionDetected = false;
		std::uint64_t csInvocations = 0; // Populated only when collectStatistics is enabled.
		bool modelSubmitted = false;
	};
	explicit MediaPipeLandscapeDirectMLEngine(Output output = Output::CpuProbabilities,
						  std::uint32_t outputWidth = kModelWidth,
						  std::uint32_t outputHeight = kModelHeight,
						  bool collectStatistics = false);
	~MediaPipeLandscapeDirectMLEngine() noexcept;

	MediaPipeLandscapeDirectMLEngine(const MediaPipeLandscapeDirectMLEngine &) = delete;
	MediaPipeLandscapeDirectMLEngine(MediaPipeLandscapeDirectMLEngine &&) = delete;
	auto operator=(const MediaPipeLandscapeDirectMLEngine &) -> MediaPipeLandscapeDirectMLEngine & = delete;
	auto operator=(MediaPipeLandscapeDirectMLEngine &&) -> MediaPipeLandscapeDirectMLEngine & = delete;

	// S_OK means submitted. Format changes fail with E_INVALIDARG without submitting work.
	// GpuMask includes refinement and resize to the constructor's output dimensions.
	// refineThreshold must be finite and in [0, 1]. CPU probabilities are unchanged.
	// Gate compares consecutive inputs independently of inference. inferMask controls
	// CPU submission of the model; gateResult describes this input's motion only.
	[[nodiscard]] auto runAsync(gs_texture_t *inputTexture, bool refineEnabled = false,
				    float refineThreshold = 0.5F,
				    double sensitivity = std::numeric_limits<double>::quiet_NaN(), bool inferMask = true) noexcept -> HRESULT;
	// Updated only by successful complete()/getResult(); no GPU wait or readback here.
	auto gateResult() const noexcept -> GateResult;
	// Reprocess retained probabilities without submitting model inference.
	[[nodiscard]] auto refineAsync(bool refineEnabled, float refineThreshold) noexcept -> HRESULT;
	// Inspect only after runAsync fails; reports whether that failure requires recreation.
	[[nodiscard]] auto needsRecreation() const noexcept -> bool;
	auto maskTexture() const noexcept -> ID3D11Texture2D *;
	[[nodiscard]] auto complete() noexcept -> HRESULT;
	[[nodiscard]] auto blockCpuUntilSubmittedGpuWorkCompletes() noexcept -> HRESULT;
	[[nodiscard]] auto isDone(bool &done) const noexcept -> HRESULT;
	[[nodiscard]] auto getResult(std::span<float> output) noexcept -> HRESULT;
	// Diagnostic access to probabilities in COPY_DEST after complete().
	auto gpuOutput() const noexcept -> ID3D12Resource *;
	auto device() const noexcept -> ID3D12Device *;
	auto commandQueue() const noexcept -> ID3D12CommandQueue *;
	// Explicit CPU blocking at the current frame's result deadline.
	[[nodiscard]] auto blockCpuUntilInferenceCompletes() noexcept -> HRESULT;

	static constexpr std::uint32_t kModelWidth = 256;
	static constexpr std::uint32_t kModelHeight = 144;

private:
	class Impl;
	std::unique_ptr<Impl> impl_;
};

} // namespace BackgroundRemoval
