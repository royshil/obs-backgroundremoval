// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <BackgroundRemoval/windows/MediaPipeLandscapeDirectML10/MPLD10Engine.hpp>
#include <windows.h>
#include <doctest.h>
#include <graphics/graphics.h>
#include <graphics/matrix4.h>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace BackgroundRemoval::MPLD10Detail {
extern const char *renderingEffectString;
}

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
	constexpr int width = 256, height = 144;
	int outputWidth = 31, outputHeight = 17;
	SUBCASE("Reduced output") {}
	SUBCASE("Expanded output")
	{
		outputWidth = 1280;
		outputHeight = 720;
	}
	const auto input = BackgroundRemoval::PipelineTest::portraitInput();
	std::vector<uint8_t> pixels(width * height * 4, 255);
	for (size_t i = 0; i < width * height; ++i)
		for (size_t c = 0; c < 3; ++c)
			pixels[i * 4 + 2 - c] = (uint8_t)std::lround(input[c * width * height + i] * 255);
	const auto *bytes = pixels.data();
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> texture{
		gs_texture_create(width, height, GS_BGRA, 1, &bytes, 0), gs_texture_destroy};
	REQUIRE(texture);
	using Engine = BackgroundRemoval::MPLD10Engine;
	Engine engine(static_cast<ID3D11Device *>(gs_get_device_obj()), Engine::Output::GpuMask);
	CHECK(engine.runAsync(static_cast<ID3D11Texture2D *>(gs_texture_get_obj(texture.get())),
			      std::numeric_limits<float>::quiet_NaN()) == E_INVALIDARG);
	CHECK(engine.runAsync(static_cast<ID3D11Texture2D *>(gs_texture_get_obj(texture.get())), -0.1F) ==
	      E_INVALIDARG);
	CHECK(engine.runAsync(static_cast<ID3D11Texture2D *>(gs_texture_get_obj(texture.get())), 1.1F) == E_INVALIDARG);
	CHECK_FALSE(engine.needsRecreation());
	REQUIRE(engine.ensureOutput() == S_OK);
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> mask{gs_texture_wrap_obj(engine.maskTexture()),
								     gs_texture_destroy};
	std::unique_ptr<gs_stagesurf_t, void (*)(gs_stagesurf_t *)> staging{
		gs_stagesurface_create(outputWidth, outputHeight, GS_RGBA), gs_stagesurface_destroy};
	REQUIRE(mask);
	REQUIRE(staging);
	CHECK(gs_texture_get_width(mask.get()) == width);
	CHECK(gs_texture_get_height(mask.get()) == height);
	CHECK(gs_texture_get_color_format(mask.get()) == GS_R32F);
	std::unique_ptr<gs_effect_t, void (*)(gs_effect_t *)> effect{
		gs_effect_create(BackgroundRemoval::MPLD10Detail::renderingEffectString, "mask-test.effect", nullptr),
		gs_effect_destroy};
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> rendered{
		gs_texture_create(outputWidth, outputHeight, GS_RGBA, 1, nullptr, GS_RENDER_TARGET),
		gs_texture_destroy};
	std::vector<std::uint8_t> white(width * height * 4, 255);
	const auto *whiteBytes = white.data();
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> source{
		gs_texture_create(width, height, GS_RGBA, 1, &whiteBytes, 0), gs_texture_destroy};
	REQUIRE(effect);
	REQUIRE(rendered);
	REQUIRE(source);
	gs_set_viewport(0, 0, outputWidth, outputHeight);
	gs_ortho(0, outputWidth, 0, outputHeight, -100, 100);
	gs_matrix_identity();
	// The pinned OBS matrix4_mul must preserve projection under an identity view.
	// This also catches MSVC 19.51 optimizing its union/SIMD accesses incorrectly.
	matrix4 view{}, projection{}, combined{};
	gs_matrix_get(&view);
	matrix4_identity(&projection);
	projection.x.x = 2.0F / outputWidth;
	projection.y.y = -2.0F / outputHeight;
	projection.t.x = -1.0F;
	projection.t.y = 1.0F;
	matrix4_mul(&combined, &view, &projection);
	const auto actualMatrix = std::bit_cast<std::array<float, 16>>(combined);
	const auto expectedMatrix = std::bit_cast<std::array<float, 16>>(projection);
	for (size_t i = 0; i < actualMatrix.size(); ++i) {
		CAPTURE(i);
		CHECK(actualMatrix[i] == expectedMatrix[i]);
	}
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
	for (float threshold : {0.0F, 0.42F, 1.0F}) {
		CAPTURE(threshold);
		REQUIRE(engine.runAsync(static_cast<ID3D11Texture2D *>(gs_texture_get_obj(texture.get())), threshold) ==
			S_OK);
		CHECK(engine.runAsync(static_cast<ID3D11Texture2D *>(gs_texture_get_obj(texture.get())), threshold) ==
		      E_PENDING);
		REQUIRE(engine.ensureOutput() == S_OK);
		gs_set_render_target_with_color_space(rendered.get(), nullptr, GS_CS_709_EXTENDED);
		while (gs_effect_loop(effect.get(), "DrawWithoutBlur")) {
			gs_effect_set_texture(gs_effect_get_param_by_name(effect.get(), "image"), source.get());
			gs_effect_set_texture(gs_effect_get_param_by_name(effect.get(), "image1"), mask.get());
			gs_draw_sprite(nullptr, 0, outputWidth, outputHeight);
		}
		gs_set_render_target(nullptr, nullptr);
		gs_stage_texture(staging.get(), rendered.get());
		uint8_t *mapped = nullptr;
		uint32_t pitch = 0;
		REQUIRE(gs_stagesurface_map(staging.get(), &mapped, &pitch));
		gs_stagesurface_unmap(staging.get());
	}
	CHECK(engine.runAsync(static_cast<ID3D11Texture2D *>(gs_texture_get_obj(texture.get())),
			      std::numeric_limits<float>::infinity()) == E_INVALIDARG);
	CHECK(engine.runAsync(static_cast<ID3D11Texture2D *>(gs_texture_get_obj(texture.get())), -1.0F) ==
	      E_INVALIDARG);
	// A full inference and postprocessing submission can be drained on destruction.
	REQUIRE(engine.runAsync(static_cast<ID3D11Texture2D *>(gs_texture_get_obj(texture.get()))) == S_OK);
}
