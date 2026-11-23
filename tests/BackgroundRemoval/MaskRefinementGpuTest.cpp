// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

// SP-01004: Mask refinement (the RefineMask pass, on the GPU through libobs)

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>

#include <DoctestRunner.hpp>

#include "Pgm.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include <graphics/graphics.h>

#include <BackgroundRemoval/MaskCompositor/MaskCompositor.hpp>

#if defined(_WIN32) && !defined(OBS_TEST_BIN_DIR)
#error OBS_TEST_BIN_DIR must be defined by the build system
#endif
#if defined(__APPLE__) && !defined(OBS_MACOS_FRAMEWORKS_DIR)
#error OBS_MACOS_FRAMEWORKS_DIR must be defined by the build system
#endif

namespace {

constexpr std::uint32_t kMaskSize = 16;

#ifdef _WIN32
constexpr const char *kGraphicsModule = OBS_TEST_BIN_DIR "/libobs-d3d11.dll";
#elif defined(__APPLE__)
// The backend of the installed OBS Studio, by its full path: os_dlopen() appends .so to a name
// without an extension.
constexpr const char *kGraphicsModule = OBS_MACOS_FRAMEWORKS_DIR "/libobs-opengl.dylib";
#else
// Renders through EGL on the X display in DISPLAY.
constexpr const char *kGraphicsModule = "libobs-opengl";
#endif

/// A libobs graphics context on the D3D11 backend on Windows and the OpenGL backend on macOS and
/// Linux, entered for its lifetime.
class GraphicsContext final {
public:
	GraphicsContext()
	{
		if (gs_create(&graphics_, kGraphicsModule, 0) != GS_SUCCESS) {
#if !defined(_WIN32) && !defined(__APPLE__)
			if (!std::getenv("DISPLAY")) {
				throw std::runtime_error(
					"gs_create failed: DISPLAY is unset; run the tests under xvfb-run");
			}
#endif
			throw std::runtime_error("gs_create failed");
		}
		gs_enter_context(graphics_);
		// As the render loop of OBS does. The OpenGL backend flips the winding when it
		// renders into a texture, so the default GS_BACK would cull every sprite.
		gs_set_cull_mode(GS_NEITHER);
	}
	~GraphicsContext()
	{
		gs_set_render_target(nullptr, nullptr);
		gs_leave_context();
		gs_destroy(graphics_);
	}
	GraphicsContext(const GraphicsContext &) = delete;
	auto operator=(const GraphicsContext &) -> GraphicsContext & = delete;

private:
	graphics_t *graphics_ = nullptr;
};

/// Enters the graphics context, created on the first call and kept until the process exits, as OBS
/// keeps its own: the compositor's effect is compiled once per process and shared.
void enterGraphics()
{
	static GraphicsContext s_graphics;
}

using UniqueTexture = std::unique_ptr<gs_texture_t, decltype(&gs_texture_destroy)>;

auto makeTexture(std::uint32_t width, std::uint32_t height, gs_color_format format, const std::uint8_t *texels)
	-> UniqueTexture
{
	const std::uint8_t *data[] = {texels};
	UniqueTexture texture(gs_texture_create(width, height, format, 1, data, 0), gs_texture_destroy);
	REQUIRE(texture);
	return texture;
}

/// Reads an R8 texture of width x height back.
auto readR8(gs_texture_t *texture, std::uint32_t width = kMaskSize, std::uint32_t height = kMaskSize)
	-> std::vector<std::uint8_t>
{
	std::unique_ptr<gs_stagesurf_t, decltype(&gs_stagesurface_destroy)> stage(
		gs_stagesurface_create(width, height, GS_R8), gs_stagesurface_destroy);
	REQUIRE(stage);
	gs_stage_texture(stage.get(), texture);
	std::uint8_t *data = nullptr;
	std::uint32_t linesize = 0;
	REQUIRE(gs_stagesurface_map(stage.get(), &data, &linesize));
	std::vector<std::uint8_t> texels(static_cast<std::size_t>(width) * height);
	for (std::uint32_t y = 0; y < height; ++y) {
		for (std::uint32_t x = 0; x < width; ++x) {
			texels[y * width + x] = data[y * linesize + x];
		}
	}
	gs_stagesurface_unmap(stage.get());
	return texels;
}

/// Refines a probability mask of width x height at the fixed foreground threshold and returns the
/// refined mask.
auto refine(const std::vector<std::uint8_t> &probability, std::uint32_t width = kMaskSize,
	    std::uint32_t height = kMaskSize) -> std::vector<std::uint8_t>
{
	const std::vector<std::uint8_t> image(static_cast<std::size_t>(width) * height * 4, 255);
	UniqueTexture imageTexture = makeTexture(width, height, GS_BGRA, image.data());
	UniqueTexture probabilityTexture = makeTexture(width, height, GS_R8, probability.data());
	BackgroundRemoval::MaskCompositor compositor;
	compositor.prepare(imageTexture.get(), probabilityTexture.get(), width, height);
	// prepare() leaves its last target bound; unbind it, as the pipeline restores its own,
	// before the compositor destroys that texture.
	gs_set_render_target(nullptr, nullptr);
	return readR8(compositor.refinedMask(), width, height);
}

/// R8 stores a share s as round(s * 255); allow one step for the conversion.
auto matchesShare(std::uint8_t texel, double share) -> bool
{
	return std::abs(static_cast<double>(texel) - share * 255.0) <= 1.0;
}

} // namespace

TEST_CASE("SP-01004.1 Interior texel")
{
	enterGraphics();
	constexpr std::uint32_t cx = 8;
	constexpr std::uint32_t cy = 8;
	std::vector<std::uint8_t> probability(kMaskSize * kMaskSize, 0);
	int set = 0;
	for (std::uint32_t y = cy - 2; y <= cy + 2 && set < 13; ++y) {
		for (std::uint32_t x = cx - 2; x <= cx + 2 && set < 13; ++x) {
			probability[y * kMaskSize + x] = 255;
			++set;
		}
	}
	const std::vector<std::uint8_t> refined = refine(probability);
	const std::uint8_t texel = refined[cy * kMaskSize + cx];
	CAPTURE(static_cast<int>(texel));
	CHECK(matchesShare(texel, 13.0 / 25.0));
	CHECK(texel >= 128);
}

TEST_CASE("SP-01004.2 Corner texel")
{
	enterGraphics();
	std::vector<std::uint8_t> probability(kMaskSize * kMaskSize, 0);
	const std::pair<std::uint32_t, std::uint32_t> foreground[] = {{0, 0}, {1, 0}, {2, 0}, {0, 1}, {1, 1}};
	for (const auto &[x, y] : foreground) {
		probability[y * kMaskSize + x] = 255;
	}
	const std::vector<std::uint8_t> refined = refine(probability);
	const std::uint8_t texel = refined[0];
	CAPTURE(static_cast<int>(texel));
	// 5 of the 9 texels inside the mask; counting the 16 outside it would give 5 / 25.
	CHECK(matchesShare(texel, 5.0 / 9.0));
	CHECK_FALSE(matchesShare(texel, 5.0 / 25.0));
}

TEST_CASE("SP-01004.4 Agreement with the reference")
{
	enterGraphics();
	const TestPgm::Gray probability = TestPgm::read(TEST_DATA_DIR "/portrait-umireon-256x144.probability.pgm");
	const TestPgm::Gray reference = TestPgm::read(TEST_DATA_DIR "/portrait-umireon-256x144.refined.pgm");
	REQUIRE(probability.width == reference.width);
	REQUIRE(probability.height == reference.height);

	const std::vector<std::uint8_t> refined = refine(probability.values, probability.width, probability.height);
	const std::vector<bool> shader =
		TestPgm::upscaledForeground(TestPgm::toUnit(refined), probability.width, probability.height, 1280, 720);
	const std::vector<bool> numpy = TestPgm::upscaledForeground(TestPgm::toUnit(reference.values), reference.width,
								    reference.height, 1280, 720);
	std::size_t agree = 0;
	for (std::size_t i = 0; i < shader.size(); ++i) {
		agree += shader[i] == numpy[i] ? 1 : 0;
	}
	const double agreement = static_cast<double>(agree) / static_cast<double>(shader.size());
	CAPTURE(agreement);
	CHECK(agreement >= 0.995);
}

int main(int argc, char **argv)
{
	doctest::Context context(argc, argv);
	return TestHelpers::runTests(context);
}
