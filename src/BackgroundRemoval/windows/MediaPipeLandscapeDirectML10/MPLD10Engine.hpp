// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#ifndef PLUGIN_PRIVATE
#define PLUGIN_PRIVATE private
#endif

#include <cstdint>
#include <memory>
#include <span>

struct ID3D11Device;
struct ID3D11Texture2D;
struct ID3D12CommandQueue;
struct ID3D12Device;
struct ID3D12Resource;

namespace BackgroundRemoval {

class MPLD10Engine final {
public:
	using HRESULT = long;

	enum class Output { CpuProbabilities, GpuMask };
	explicit MPLD10Engine(ID3D11Device *device, Output output = Output::CpuProbabilities);
	~MPLD10Engine() noexcept;

	MPLD10Engine(const MPLD10Engine &) = delete;
	MPLD10Engine(MPLD10Engine &&) = delete;
	auto operator=(const MPLD10Engine &) -> MPLD10Engine & = delete;
	auto operator=(MPLD10Engine &&) -> MPLD10Engine & = delete;

	// S_OK means queued. Format changes fail with E_INVALIDARG without queuing work.
	// GpuMask exposes a 256x144 FP32 Box-filtered mask for D3D11 sampling.
	// refineThreshold must be finite and in [0, 1]. CPU probabilities are unchanged.
	// Before reusing GpuMask inputs/settings, the caller must have completed the
	// preceding D3D11 consumption (for example, a completed staging readback).
	// The owner submits the D3D11 stream (for example, an end-of-frame flush); runAsync does not flush it.
	// CPU-output callers must submit that stream before blocking for inference.
	[[nodiscard]] auto runAsync(ID3D11Texture2D *inputTexture, float refineThreshold = 0.5F) noexcept -> HRESULT;
	// Inspect only after runAsync fails; reports whether that failure requires recreation.
	[[nodiscard]] auto needsRecreation() const noexcept -> bool;
	auto maskTexture() const noexcept -> ID3D11Texture2D *;
	// Queue a D3D11 GPU wait before consuming the mask. Never waits on the CPU.
	// Repeated calls without a new submission are no-ops, including the initial mask.
	[[nodiscard]] auto ensureOutput() noexcept -> HRESULT;
	// CPU-output diagnostics only; not needed by the GPU-mask rendering path.
	[[nodiscard]] auto isDone(bool &done) const noexcept -> HRESULT;
	[[nodiscard]] auto getResult(std::span<float> output) noexcept -> HRESULT;
	// Diagnostic access to the model-sized FP32 mask buffer in COPY_DEST.
	// The diagnostic caller must establish GPU completion before CPU readback.
	auto gpuOutput() const noexcept -> ID3D12Resource *;
	auto device() const noexcept -> ID3D12Device *;
	auto commandQueue() const noexcept -> ID3D12CommandQueue *;
	// Explicit CPU blocking for CPU-output diagnostics, never used by Pipeline.
	[[nodiscard]] auto blockCpuUntilInferenceCompletes() noexcept -> HRESULT;

	static constexpr std::uint32_t kModelWidth = 256;
	static constexpr std::uint32_t kModelHeight = 144;

PLUGIN_PRIVATE:
	class Impl;
	std::unique_ptr<Impl> impl_;
};

} // namespace BackgroundRemoval
