// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "BackgroundRemoval/windows/MediaPipeLandscapeDirectMLEngine.hpp"
#include <windows.h>
#include <doctest.h>
#include <graphics/graphics.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace BackgroundRemoval::PipelineTest {
auto portraitInput() -> std::vector<float>;
}

TEST_CASE("LandscapeMaskEffect")
{
	graphics_t *graphics = nullptr;
	REQUIRE(gs_create(&graphics, OBS_TEST_BIN_DIR "/libobs-d3d11.dll", 0) == 0);
	struct Cleanup {
		graphics_t *graphics;
		~Cleanup()
		{
			gs_set_render_target(nullptr, nullptr);
			gs_leave_context();
			gs_destroy(graphics);
		}
	} cleanup{graphics};
	gs_enter_context(graphics);
	constexpr int width = 256, height = 144, outputWidth = 31, outputHeight = 17;
	const auto input = BackgroundRemoval::PipelineTest::portraitInput();
	std::vector<uint8_t> pixels(width * height * 4, 255);
	for (size_t i = 0; i < width * height; ++i)
		for (size_t c = 0; c < 3; ++c)
			pixels[i * 4 + 2 - c] = (uint8_t)std::lround(input[c * width * height + i] * 255);
	const auto *bytes = pixels.data();
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> texture{
		gs_texture_create(width, height, GS_BGRA, 1, &bytes, 0), gs_texture_destroy};
	REQUIRE(texture);
	using Engine = BackgroundRemoval::MediaPipeLandscapeDirectMLEngine;
	Engine reference;
	std::vector<float> probabilities(width * height);
	REQUIRE(reference.runAsync(texture.get()) == S_OK);
	REQUIRE(reference.blockCpuUntilSubmittedGpuWorkCompletes() == S_OK);
	REQUIRE(reference.getResult(probabilities) == S_OK);
	Engine engine(Engine::Output::GpuMask, outputWidth, outputHeight);
	REQUIRE(engine.refineAsync(true, 0.5F) == E_UNEXPECTED);
	CHECK(engine.runAsync(texture.get(), true, std::numeric_limits<float>::quiet_NaN()) == E_INVALIDARG);
	CHECK(engine.runAsync(texture.get(), true, -0.1F) == E_INVALIDARG);
	CHECK(engine.runAsync(texture.get(), true, 1.1F) == E_INVALIDARG);
	CHECK_FALSE(engine.needsRecreation());
	REQUIRE(engine.runAsync(texture.get(), true, 0.42F) == S_OK);
	CHECK(engine.refineAsync(false, 0.5F) == E_PENDING);
	REQUIRE(engine.blockCpuUntilSubmittedGpuWorkCompletes() == S_OK);
	REQUIRE(engine.complete() == S_OK);
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> mask{gs_texture_wrap_obj(engine.maskTexture()),
								     gs_texture_destroy};
	std::unique_ptr<gs_stagesurf_t, void (*)(gs_stagesurf_t *)> staging{
		gs_stagesurface_create(outputWidth, outputHeight, GS_R8), gs_stagesurface_destroy};
	REQUIRE(mask);
	REQUIRE(staging);
	CHECK(gs_texture_get_width(mask.get()) == outputWidth);
	CHECK(gs_texture_get_height(mask.get()) == outputHeight);
	constexpr std::array<uint32_t, 9> weights{4, 13, 30, 51, 60, 51, 30, 13, 4};
	for (bool enabled : {false, true}) {
		for (float threshold : {0.0F, 0.42F, 1.0F}) {
			CAPTURE(enabled);
			CAPTURE(threshold);
			if (threshold == 0.42F) {
				REQUIRE(engine.runAsync(texture.get(), enabled, threshold) == S_OK);
			} else {
				REQUIRE(engine.refineAsync(enabled, threshold) == S_OK);
			}
			CHECK(engine.refineAsync(enabled, threshold) == E_PENDING);
			REQUIRE(engine.blockCpuUntilSubmittedGpuWorkCompletes() == S_OK);
			REQUIRE(engine.complete() == S_OK);
			std::vector<uint32_t> values(width * height);
			for (size_t i = 0; i < values.size(); ++i)
				values[i] = (uint32_t)std::nearbyint(std::clamp(1.0F - probabilities[i], 0.0F, 1.0F) *
								     255.0F);
			if (enabled) {
				for (auto &value : values)
					value = value / 255.0F > 1.0F - threshold ? 255 : 0;
				for (int pass = 0; pass < 2; ++pass) {
					const auto previous = values;
					for (int y = 0; y < height; ++y)
						for (int x = 0; x < width; ++x) {
							uint32_t sum = 0;
							for (int k = -4; k <= 4; ++k) {
								int sx = std::clamp(x + (pass == 0 ? k : 0), 0,
										    width - 1);
								int sy = std::clamp(y + (pass == 1 ? k : 0), 0,
										    height - 1);
								sum += previous[sy * width + sx] * weights[k + 4];
							}
							values[y * width + x] = (sum + 128) / 256;
						}
				}
			}
			gs_stage_texture(staging.get(), mask.get());
			uint8_t *mapped = nullptr;
			uint32_t pitch = 0;
			REQUIRE(gs_stagesurface_map(staging.get(), &mapped, &pitch));
			int maximum = 0;
			auto at = [&](int x, int y) {
				return float(
					values[std::clamp(y, 0, height - 1) * width + std::clamp(x, 0, width - 1)]);
			};
			for (int y = 0; y < outputHeight; ++y)
				for (int x = 0; x < outputWidth; ++x) {
					float sx = (x + 0.5F) * width / outputWidth - 0.5F,
					      sy = (y + 0.5F) * height / outputHeight - 0.5F;
					int ix = int(std::floor(sx)), iy = int(std::floor(sy));
					float fx = sx - ix, fy = sy - iy;
					float v = ((at(ix, iy) * (1 - fx) + at(ix + 1, iy) * fx) * (1 - fy) +
						   (at(ix, iy + 1) * (1 - fx) + at(ix + 1, iy + 1) * fx) * fy) /
						  255;
					int expected = enabled ? (v > 0.5F ? 255 : 0) : int(std::nearbyint(v * 255));
					maximum = std::max(maximum, std::abs(int(mapped[y * pitch + x]) - expected));
				}
			gs_stagesurface_unmap(staging.get());
			CHECK(maximum <= (enabled ? 0 : 1));
		}
	}
	CHECK(engine.refineAsync(true, std::numeric_limits<float>::infinity()) == E_INVALIDARG);
	CHECK(engine.refineAsync(true, -1.0F) == E_INVALIDARG);
	CHECK(reference.refineAsync(true, 0.5F) == E_UNEXPECTED);
	// A retained-output submission can also be drained safely during destruction.
	REQUIRE(engine.refineAsync(true, 0.5F) == S_OK);
}
