// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ObsBridgeUtils/GsUnique.hpp"

struct ID3D12Device;
struct ID3D12CommandQueue;
struct ID3D12Resource;

namespace BackgroundRemoval::MediaPipeLandscapeDirectML {
// GPU-only texture transfer. Use in the OBS graphics context, on one thread.
// The supplied queue must consume the texture before signalConsumed() and must
// complete that submission before reuse/destruction. Texture state is COMMON
// at both boundaries. This class never maps or reads image pixels on the CPU.
class D3D12TextureReader final {
public:
	D3D12TextureReader(ID3D12Device *device, ID3D12CommandQueue *queue, std::uint32_t width, std::uint32_t height);
	~D3D12TextureReader();
	D3D12TextureReader(const D3D12TextureReader &) = delete;
	D3D12TextureReader &operator=(const D3D12TextureReader &) = delete;
	auto capture(gs_texture_t *source) -> ID3D12Resource *;
	void signalConsumed();

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};
} // namespace BackgroundRemoval::MediaPipeLandscapeDirectML
