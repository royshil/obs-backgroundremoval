// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "DmlProgram/BaselineDmlProgramRuntime.hpp"
#include "D3D12TextureReader.hpp"

namespace BackgroundRemoval::MediaPipeLandscapeDirectML {
struct GpuImageTimings final {
	double captureCpuMs = 0, recordCpuMs = 0, submitCpuMs = 0, waitCpuMs = 0, readbackCpuMs = 0;
	double packGpuMs = 0, gateGpuMs = 0, modelGpuMs = 0, packetGpuMs = 0;
};
// Single Float32 image input. Predicate and model output are read in one
// completed submission; input and PSNR never cross to CPU memory.
class GpuImageInference final {
public:
	GpuImageInference(BaselineDml::BaselineDmlProgramRuntime &runtime, std::uint32_t width, std::uint32_t height);
	~GpuImageInference();
	GpuImageInference(const GpuImageInference &) = delete;
	GpuImageInference &operator=(const GpuImageInference &) = delete;
	auto run(D3D12TextureReader &reader, gs_texture_t *input, float threshold, bool forceInference,
		 std::span<float> output) -> bool;
	// Call only after CPU mask refinement succeeds. Skipped frames never commit.
	void commitReference();
	// Opt-in diagnostic timestamps; disabled in the production pipeline.
	void enableProfiling();
	auto lastTimings() const noexcept -> GpuImageTimings;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};
} // namespace BackgroundRemoval::MediaPipeLandscapeDirectML
