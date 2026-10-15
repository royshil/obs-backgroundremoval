// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <memory>
struct ID3D12Device;
struct ID3D12CommandQueue;
struct ID3D12Resource;
struct ID3D11Device;
struct ID3D11Texture2D;
namespace BackgroundRemoval {
class MaskRefinerDirect3D final {
public:
	struct Settings final {
		bool enabled = false;
		float threshold = 0.5F;
		std::int32_t expansion = 0;
	};
	MaskRefinerDirect3D(ID3D12Device *device, ID3D12CommandQueue *queue, ID3D11Device *graphicsDevice,
			    std::uint32_t inputWidth, std::uint32_t inputHeight, std::uint32_t outputWidth,
			    std::uint32_t outputHeight);
	~MaskRefinerDirect3D();
	MaskRefinerDirect3D(const MaskRefinerDirect3D &) = delete;
	auto operator=(const MaskRefinerDirect3D &) -> MaskRefinerDirect3D & = delete;
	// Input is a packed FP32 probability buffer in COPY_DEST, on this device/queue.
	// nullptr reuses the retained probabilities (initially all background).
	// The caller must serialize calls with the OBS graphics context.
	void runAsync(ID3D12Resource *probabilities, const Settings &settings);
	auto isDone() const -> bool;
	void blockCpuUntilComplete();
	// Requires completion. Reads only a four-byte validation flag, never mask pixels.
	void complete();
	auto outputTexture() const noexcept -> ID3D11Texture2D *;

private:
	class Impl;
	std::unique_ptr<Impl> impl_;
};
} // namespace BackgroundRemoval
