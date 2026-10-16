// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <memory>
#include <span>

struct ID3D11Texture2D;
struct ID3D12CommandQueue;
struct ID3D12Device;
struct ID3D12Resource;

struct gs_texture;
typedef struct gs_texture gs_texture_t;

namespace BackgroundRemoval {

class MPLDEngine final {
public:
	using HRESULT = long;

	enum class Output { CpuProbabilities, GpuMask };
	explicit MPLDEngine(Output output = Output::CpuProbabilities,
						  std::uint32_t outputWidth = kModelWidth,
						  std::uint32_t outputHeight = kModelHeight);
	~MPLDEngine() noexcept;

	MPLDEngine(const MPLDEngine &) = delete;
	MPLDEngine(MPLDEngine &&) = delete;
	auto operator=(const MPLDEngine &) -> MPLDEngine & = delete;
	auto operator=(MPLDEngine &&) -> MPLDEngine & = delete;

	// S_OK means submitted. Format changes fail with E_INVALIDARG without submitting work.
	// GpuMask includes refinement and resize to the constructor's output dimensions.
	// refineThreshold must be finite and in [0, 1]. CPU probabilities are unchanged.
	[[nodiscard]] auto runAsync(gs_texture_t *inputTexture, bool refineEnabled = false,
				    float refineThreshold = 0.5F) noexcept -> HRESULT;
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
