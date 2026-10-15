// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <memory>
#include <span>

struct ID3D12Device;
struct ID3D12CommandQueue;
struct ID3D12Resource;

struct gs_texture;
typedef struct gs_texture gs_texture_t;

namespace BackgroundRemoval {

class MediaPipeLandscapeDirectMLEngine final {
public:
	explicit MediaPipeLandscapeDirectMLEngine(bool gpuOutput = false);
	~MediaPipeLandscapeDirectMLEngine();

	MediaPipeLandscapeDirectMLEngine(const MediaPipeLandscapeDirectMLEngine &) = delete;
	MediaPipeLandscapeDirectMLEngine(MediaPipeLandscapeDirectMLEngine &&) = delete;
	auto operator=(const MediaPipeLandscapeDirectMLEngine &) -> MediaPipeLandscapeDirectMLEngine & = delete;
	auto operator=(MediaPipeLandscapeDirectMLEngine &&) -> MediaPipeLandscapeDirectMLEngine & = delete;

	// One submission at a time. Calls are serialized in the caller's OBS graphics context.
	// Keep the input texture alive until the result is retrieved.
	// The first input fixes the native format; subsequent inputs must match it.
	enum class RunResult { Submitted, NeedsRecreation };
	auto runAsync(gs_texture_t *inputTexture) -> RunResult;
	auto isDone() const -> bool;
	void getResult(std::span<float> output);
	// Borrowed GPU output in COPY_DEST. Consumers must use the same device/queue.
	auto gpuOutput() const -> ID3D12Resource *;
	auto device() const noexcept -> ID3D12Device *;
	auto commandQueue() const noexcept -> ID3D12CommandQueue *;
	void releaseGpuResult();
	// Explicit CPU blocking at the current frame's result deadline.
	void blockCpuUntilInferenceCompletes();

	static constexpr std::uint32_t kModelWidth = 256;
	static constexpr std::uint32_t kModelHeight = 144;

private:
	class Impl;
	std::unique_ptr<Impl> impl_;
};

} // namespace BackgroundRemoval
