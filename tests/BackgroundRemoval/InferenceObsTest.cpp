// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

// SP-01005: MediaPipe Selfie Segmenter inference, by the shader pipeline running in libobs,
// against ONNX Runtime's output in tests/data.

#include <doctest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <graphics/graphics.h>
#include <graphics/vec4.h>

#include <BackgroundRemoval/MediaPipeLandscapeShader/MPLSPipeline.hpp>

#include "ObsHarness.hpp"
#include "Pgm.hpp"

using namespace ObsHarness;

namespace {

constexpr std::uint32_t kWidth = BackgroundRemoval::MPLSPipeline::kModelWidth;
constexpr std::uint32_t kHeight = BackgroundRemoval::MPLSPipeline::kModelHeight;

/// Runs the shader pipeline once on the image source and returns its foreground probability.
/// The pipeline renders the target of the filter it is given; a Color Correction filter with
/// default settings serves only as that handle and is not rendered.
auto inferProbability(obs_source_t *filter) -> std::vector<std::uint8_t>
{
	std::vector<std::uint8_t> probability(static_cast<std::size_t>(kWidth) * kHeight);
	obs_enter_graphics();
	{
		BackgroundRemoval::MPLSPipeline pipeline(filter, kWidth, kHeight, 1);

		gs_texrender_t *texrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
		if (gs_texrender_begin(texrender, kWidth, kHeight)) {
			gs_ortho(0.0F, static_cast<float>(kWidth), 0.0F, static_cast<float>(kHeight), -100.0F, 100.0F);
			pipeline.videoRender();
			gs_texrender_end(texrender);
		}
		gs_texrender_destroy(texrender);

		gs_stagesurf_t *stage = gs_stagesurface_create(kWidth, kHeight, GS_R8);
		gs_stage_texture(stage, pipeline.probability());
		std::uint8_t *data = nullptr;
		std::uint32_t linesize = 0;
		if (gs_stagesurface_map(stage, &data, &linesize)) {
			for (std::uint32_t y = 0; y < kHeight; ++y) {
				for (std::uint32_t x = 0; x < kWidth; ++x) {
					probability[y * kWidth + x] = data[y * linesize + x];
				}
			}
			gs_stagesurface_unmap(stage);
		}
		gs_stagesurface_destroy(stage);
	}
	obs_leave_graphics();
	return probability;
}

} // namespace

TEST_CASE("SP-01005.2 The foreground mask matches ONNX Runtime")
{
	const TestPgm::Gray reference = TestPgm::read(testDataFile("portrait-umireon-256x144.probability.pgm"));
	REQUIRE(reference.width == kWidth);
	REQUIRE(reference.height == kHeight);

	const Scene scene(testDataFile("portrait-umireon-256x144.png"), {{"color_filter_v2", "{}"}});
	REQUIRE(scene.filter(0) != nullptr);
	REQUIRE(obs_source_get_base_width(scene.source()) == kWidth);
	const std::vector<std::uint8_t> probability = inferProbability(scene.filter(0));

	std::size_t intersection = 0;
	std::size_t unionCount = 0;
	for (std::size_t i = 0; i < probability.size(); ++i) {
		const bool shader = probability[i] >= 128;
		const bool onnx = reference.values[i] >= 128;
		intersection += shader && onnx ? 1 : 0;
		unionCount += shader || onnx ? 1 : 0;
	}
	REQUIRE(unionCount > 0);
	const double iou = static_cast<double>(intersection) / static_cast<double>(unionCount);
	CAPTURE(iou);
	CHECK(iou >= 0.99);
}
