// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <memory>
#include <span>

struct gs_texture;
typedef struct gs_texture gs_texture_t;

namespace BackgroundRemoval {

class MediaPipeLandscapeDirectMLEngine final {
public:
	MediaPipeLandscapeDirectMLEngine();
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
	// Explicit CPU blocking at the current frame's result deadline.
	void blockCpuUntilInferenceCompletes();

	static constexpr std::uint32_t kModelWidth = 256;
	static constexpr std::uint32_t kModelHeight = 144;

private:
	class Impl;
	std::unique_ptr<Impl> impl_;
};

} // namespace BackgroundRemoval
