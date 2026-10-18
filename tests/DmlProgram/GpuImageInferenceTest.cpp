#include "MaskReference.hpp"
#include "LandscapeTestModel.hpp"
#include <BackgroundRemoval/windows/PipelineFactory.hpp>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <TraceLoggingProvider.h>

TRACELOGGING_DECLARE_PROVIDER(g_hProvider);
// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>
#include "GpuAccessValidation.hpp"
#include <DmlProgram/BaselineDmlProgramRuntime.hpp>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>
#include <util/platform.h>

void runLandscapeReference(BaselineDml::BaselineDmlProgramRuntime &runtime, std::span<const float> input,
			   std::span<float> output)
{
	if (runtime.inputInfo(0).type != BaselineDml::ProgramDefinition::dtFloat32 ||
	    runtime.outputInfo().type != BaselineDml::ProgramDefinition::dtFloat32) {
		FAIL("Landscape requires FP32 input and output");
	}
	const std::array inputs{std::as_bytes(input)};
	runtime.run(inputs, std::as_writable_bytes(output));
}

#ifdef GPU_LANDSCAPE_MODEL
#include <BackgroundRemoval/windows/MPLD10Pipeline.hpp>
#include <BackgroundRemoval/windows/MediaPipeLandscapeDirectML10/MPLD10Engine.hpp>
#include <wincodec.h>
#include <winrt/base.h>
#endif

namespace {
using Microsoft::WRL::ComPtr;
struct Graphics {
	graphics_t *context = nullptr;
	Graphics()
	{
		if (gs_create(&context, OBS_TEST_BIN_DIR "/libobs-d3d11.dll", 0) != 0)
			throw std::runtime_error("Cannot create OBS D3D11 graphics context");
		gs_enter_context(context);
	}
	~Graphics()
	{
		gs_leave_context();
		gs_destroy(context);
	}
};

} // namespace

#ifdef GPU_LANDSCAPE_MODEL
namespace BackgroundRemoval {
struct MPLD10PipelineTestAccess {
	static auto &staging(MPLD10Pipeline &pipeline) { return pipeline.lumaAbsDiffStagesurf_; }
	static void stageDifference(MPLD10Pipeline &pipeline, gs_texture_t *texture)
	{
		if (texture)
			gs_stage_texture(pipeline.lumaAbsDiffStagesurf_.get(), texture);
		pipeline.differenceTimestamp_ = os_gettime_ns();
		pipeline.differenceStaged_ = true;
	}
	static void setRemaining(MPLD10Pipeline &pipeline, std::uint32_t count)
	{
		pipeline.remainingInferenceFrames_ = count;
	}
	static void invalidateStaging(MPLD10Pipeline &pipeline) { pipeline.lumaAbsDiffStagesurf_.reset(); }
	static auto staged(const MPLD10Pipeline &pipeline) -> bool { return pipeline.differenceStaged_; }
	static auto remaining(const MPLD10Pipeline &pipeline) -> std::uint32_t
	{
		return pipeline.remainingInferenceFrames_;
	}
	static void replaceEngine(MPLD10Pipeline &pipeline, std::unique_ptr<MPLD10Engine> engine)
	{
		pipeline.engine_ = std::move(engine);
	}
	static auto alphaTexture(const MPLD10Pipeline &pipeline) -> gs_texture_t *
	{
		return pipeline.alphaTexture_.get();
	}
	static auto lastInputTexture(const MPLD10Pipeline &pipeline) -> gs_texture_t *
	{
		return pipeline.lastInputTexture_.get();
	}
};
} // namespace BackgroundRemoval
namespace BackgroundRemoval::PipelineTest {
auto portraitInput() -> std::vector<float>
{
	ComPtr<IWICImagingFactory> factory;
	winrt::check_hresult(
		CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)));
	ComPtr<IWICBitmapDecoder> decoder;
	winrt::check_hresult(factory->CreateDecoderFromFilename(winrt::to_hstring(LANDSCAPE_PORTRAIT).c_str(), nullptr,
								GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder));
	ComPtr<IWICBitmapFrameDecode> frame;
	winrt::check_hresult(decoder->GetFrame(0, &frame));
	ComPtr<IWICBitmapScaler> scaler;
	winrt::check_hresult(factory->CreateBitmapScaler(&scaler));
	winrt::check_hresult(scaler->Initialize(frame.Get(), 256, 144, WICBitmapInterpolationModeLinear));
	ComPtr<IWICFormatConverter> converter;
	winrt::check_hresult(factory->CreateFormatConverter(&converter));
	winrt::check_hresult(converter->Initialize(scaler.Get(), GUID_WICPixelFormat24bppRGB, WICBitmapDitherTypeNone,
						   nullptr, 0, WICBitmapPaletteTypeCustom));
	std::vector<std::uint8_t> rgb(256 * 144 * 3);
	winrt::check_hresult(converter->CopyPixels(nullptr, 256 * 3, static_cast<UINT>(rgb.size()), rgb.data()));
	std::vector<float> input(rgb.size());
	for (std::size_t i = 0; i < 256 * 144; ++i)
		for (std::size_t c = 0; c < 3; ++c)
			input[c * 256 * 144 + i] = rgb[3 * i + c] / 255.0F;
	return input;
}

struct SourceData {
	gs_texture_t *image_ = nullptr;
	std::size_t renderCount_ = 0;
	bool reenter_ = false;
	MPLD10Pipeline *pipeline_ = nullptr;
};
auto name(void *) -> const char *
{
	return "DirectML Pipeline fixture";
}
SourceData *inputData = nullptr;
SourceData *filterData = nullptr;
auto create(obs_data_t *, obs_source_t *source) -> void *
{
	auto *data = new SourceData;
	if (std::strcmp(obs_source_get_id(source), "dml-pipeline-test-input") == 0)
		inputData = data;
	else
		filterData = data;
	return data;
}
void destroy(void *data)
{
	delete static_cast<SourceData *>(data);
}
auto width(void *) -> uint32_t
{
	return 256;
}
auto height(void *) -> uint32_t
{
	return 144;
}
auto colorSpace(void *, size_t, const gs_color_space *) -> gs_color_space
{
	return GS_CS_709_EXTENDED;
}
void renderSource(void *data, gs_effect_t *)
{
	auto *source = static_cast<SourceData *>(data);
	++source->renderCount_;
	if (source->reenter_) {
		source->reenter_ = false;
		source->pipeline_->videoRender(nullptr);
	}
	if (gs_get_effect())
		obs_source_draw(static_cast<SourceData *>(data)->image_, 0, 0, 256, 144, false);
}
void renderFilter(void *data, gs_effect_t *effect)
{
	static_cast<SourceData *>(data)->pipeline_->videoRender(effect);
}
} // namespace BackgroundRemoval::PipelineTest
#endif

#ifdef GPU_LANDSCAPE_MODEL
TEST_CASE("LandscapeRenderingInitialization")
{
	ComPtr<ID3D12Device> validationDevice;
	if (GpuAccessValidation::enabled()) {
		REQUIRE(SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&validationDevice))));
		ComPtr<ID3D12InfoQueue> messages;
		REQUIRE(SUCCEEDED(validationDevice.As(&messages)));
		REQUIRE(SUCCEEDED(messages->SetMessageCountLimit(65536)));
	}
	struct ObsSession {
		ObsSession()
		{
			if (!obs_startup("en-US", nullptr, nullptr))
				throw std::runtime_error("OBS startup failed");
		}
		~ObsSession() { obs_shutdown(); }
	} session;
	obs_add_data_path(OBS_TEST_BIN_DIR "/../../data/libobs/");
	obs_video_info video{};
	video.graphics_module = OBS_TEST_BIN_DIR "/libobs-d3d11.dll";
	video.fps_num = 30;
	video.fps_den = 1;
	video.base_width = video.output_width = 256;
	video.base_height = video.output_height = 144;
	video.output_format = VIDEO_FORMAT_RGBA;
	video.colorspace = VIDEO_CS_709;
	video.range = VIDEO_RANGE_FULL;
	REQUIRE(obs_reset_video(&video) == OBS_VIDEO_SUCCESS);
	// video_tick creates the pipeline without an active graphics context.
	{
		std::unique_ptr<obs_source_t, void (*)(obs_source_t *)> source{
			obs_source_create_private("scene", "Pipeline factory test", nullptr), obs_source_release};
		REQUIRE(source);
		BackgroundRemoval::FilterProperty property{};
		property.pipelineId = BackgroundRemoval::MPLD10Pipeline::kPipelineId;
		auto pipeline = BackgroundRemoval::createRenderingPipeline(property, source.get(), 256, 144);
		REQUIRE(pipeline);
		CHECK(pipeline->getWidth() == 256);
		CHECK(pipeline->getHeight() == 144);
	}
	obs_enter_graphics();
	struct LeaveGraphics {
		~LeaveGraphics() { obs_leave_graphics(); }
	} graphics;
	auto *scene = obs_source_create_private("scene", "Landscape initialization test", nullptr);
	REQUIRE(scene);
	struct ReleaseSource {
		obs_source_t *value;
		~ReleaseSource() { obs_source_release(value); }
	} source{scene};
	auto *previousTarget = gs_get_render_target();
	auto previousSpace = gs_get_color_space();
	BackgroundRemoval::FilterProperty property{};
	std::unique_ptr<obs_data_t, void (*)(obs_data_t *)> settings{obs_data_create(), obs_data_release};
	property.imageSimilarityThreshold = 42.0;
	BackgroundRemoval::encodeFilterProperty(settings.get(), property);
	CHECK_FALSE(obs_data_has_user_value(settings.get(), "v2_RefineMask_Expansion"));
	// Historical Windows settings are ignored and are not serialized again.
	obs_data_set_int(settings.get(), "v2_RefineMask_Expansion", 30);
	std::unique_ptr<obs_data_t, void (*)(obs_data_t *)> roundTrip{obs_data_create(), obs_data_release};
	BackgroundRemoval::encodeFilterProperty(roundTrip.get(),
						BackgroundRemoval::decodeFilterProperty(settings.get()));
	CHECK_FALSE(obs_data_has_user_value(roundTrip.get(), "v2_RefineMask_Expansion"));
	CHECK(BackgroundRemoval::decodeFilterProperty(settings.get()).imageSimilarityThreshold == 42.0);
	{
		BackgroundRemoval::MPLD10Pipeline pipeline(property, scene, 256, 144);
		CHECK(pipeline.getWidth() == 256);
		CHECK(pipeline.getHeight() == 144);
		CHECK(gs_get_render_target() == previousTarget);
		CHECK(gs_get_color_space() == previousSpace);
	}
	using namespace BackgroundRemoval::PipelineTest;
	obs_source_info inputInfo{};
	inputInfo.id = "dml-pipeline-test-input";
	inputInfo.type = OBS_SOURCE_TYPE_INPUT;
	inputInfo.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW;
	inputInfo.get_name = name;
	inputInfo.create = create;
	inputInfo.destroy = destroy;
	inputInfo.get_width = width;
	inputInfo.get_height = height;
	inputInfo.video_render = renderSource;
	inputInfo.video_get_color_space = colorSpace;
	obs_register_source(&inputInfo);
	obs_source_info filterInfo{};
	filterInfo.id = "dml-pipeline-test-filter";
	filterInfo.type = OBS_SOURCE_TYPE_FILTER;
	filterInfo.output_flags = OBS_SOURCE_VIDEO;
	filterInfo.get_name = name;
	filterInfo.create = create;
	filterInfo.destroy = destroy;
	filterInfo.video_render = renderFilter;
	filterInfo.video_get_color_space = colorSpace;
	obs_register_source(&filterInfo);
	std::unique_ptr<obs_source_t, void (*)(obs_source_t *)> inputSource{
		obs_source_create_private(inputInfo.id, "Input", nullptr), obs_source_release};
	std::unique_ptr<obs_source_t, void (*)(obs_source_t *)> filter{
		obs_source_create_private(filterInfo.id, "Filter", nullptr), obs_source_release};
	REQUIRE(inputSource);
	REQUIRE(filter);
	obs_source_filter_add(inputSource.get(), filter.get());

	auto input = portraitInput();
	std::vector<uint8_t> pixels(256 * 144 * 4, 255);
	for (size_t i = 0; i < 256 * 144; ++i)
		for (size_t c = 0; c < 3; ++c)
			pixels[4 * i + 2 - c] = uint8_t(std::round(input[c * 256 * 144 + i] * 255));
	const auto *bytes = pixels.data();
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> image{
		gs_texture_create(256, 144, GS_BGRA, 1, &bytes, 0), gs_texture_destroy};
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> destination{
		gs_texture_create(256, 144, GS_BGRA, 1, nullptr, GS_RENDER_TARGET), gs_texture_destroy};
	std::unique_ptr<gs_stagesurf_t, void (*)(gs_stagesurf_t *)> readback{gs_stagesurface_create(256, 144, GS_BGRA),
									     gs_stagesurface_destroy};
	REQUIRE(image);
	REQUIRE(destination);
	REQUIRE(readback);
	inputData->image_ = image.get();
	auto runtime = LandscapeTestModel::makeRawRuntime();
	std::vector<float> expected(256 * 144);
	const std::array inputs{std::as_bytes(std::span(input))};
	runLandscapeReference(*runtime, input, expected);
	property.imageSimilarityThreshold = 40;
	{
		BackgroundRemoval::MPLD10Pipeline pipeline(property, filter.get(), 256, 144);
		filterData->pipeline_ = &pipeline;
		inputData->pipeline_ = &pipeline;
		CHECK_FALSE(pipeline.needsRecreation());
		// A black first input skips inference and keeps the initial background mask.
		std::vector<uint8_t> blackPixels(256 * 144 * 4, 0);
		for (size_t i = 0; i < 256 * 144; ++i)
			blackPixels[i * 4 + 3] = 255;
		const auto *blackBytes = blackPixels.data();
		std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> black{
			gs_texture_create(256, 144, GS_BGRA, 1, &blackBytes, 0), gs_texture_destroy};
		REQUIRE(black);
		inputData->image_ = black.get();
		gs_set_render_target_with_color_space(destination.get(), nullptr, GS_CS_709_EXTENDED);
		gs_set_viewport(0, 0, 256, 144);
		gs_ortho(0, 256, 0, 144, -100, 100);
		gs_matrix_identity();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
		obs_source_video_render(inputSource.get());
		CHECK_FALSE(pipeline.needsRecreation());
		gs_stage_texture(readback.get(), destination.get());
		uint8_t *initialData = nullptr;
		uint32_t initialPitch = 0;
		REQUIRE(gs_stagesurface_map(readback.get(), &initialData, &initialPitch));
		bool initialBackground = true;
		for (size_t y = 0; y < 144; ++y)
			for (size_t x = 0; x < 256; ++x)
				initialBackground &= initialData[y * initialPitch + x * 4 + 3] == 0;
		gs_stagesurface_unmap(readback.get());
		CHECK(initialBackground);
		const auto blackRenderCount = inputData->renderCount_;
		obs_source_video_render(inputSource.get());
		CHECK(inputData->renderCount_ == blackRenderCount + 1);
		inputData->image_ = image.get();
		constexpr int frameCount = 4;
		std::vector<float> heldExpected(expected.size(), 0.0F);
		for (int frame = 0; frame < frameCount; ++frame) {
			pipeline.videoTick(1.0F / 30);
			if (frame >= 3) {
				for (auto &value : input)
					value = 1 - value;
				for (size_t i = 0; i < 256 * 144; ++i)
					for (size_t c = 0; c < 3; ++c)
						pixels[4 * i + 2 - c] =
							uint8_t(std::round(input[c * 256 * 144 + i] * 255));
				const auto *inverted = pixels.data();
				image.reset(gs_texture_create(256, 144, GS_BGRA, 1, &inverted, 0));
				REQUIRE(image);
				inputData->image_ = image.get();
				runLandscapeReference(*runtime, input, expected);
			}
			// Settings do not force inference; mandatory refinement accompanies the next inference.
			property.blurBackgroundEnabled = frame == 1;
			property.blurBackgroundFactor = 2;
			property.refineMaskEnabled = frame == 1;
			property.refineMaskThreshold = 0.5;
			pipeline.update(property);
			gs_set_render_target_with_color_space(destination.get(), nullptr, GS_CS_709_EXTENDED);
			gs_set_viewport(0, 0, 256, 144);
			gs_ortho(0, 256, 0, 144, -100, 100);
			gs_matrix_identity();
			gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
			// Processing must reject reentrant renders without touching graphics state.
			inputData->reenter_ = true;
			const auto beforeRendering = inputData->renderCount_;
			const bool expectedInference =
				BackgroundRemoval::MPLD10PipelineTestAccess::remaining(pipeline) > 0;
			obs_source_video_render(inputSource.get());
			if (expectedInference)
				heldExpected = expected;
			CHECK_FALSE(inputData->reenter_);
			CHECK(inputData->renderCount_ == beforeRendering + 1);
			CHECK(gs_get_render_target() == destination.get());
			CHECK(gs_get_color_space() == GS_CS_709_EXTENDED);
			gs_rect viewport{};
			gs_get_viewport(&viewport);
			CHECK(viewport.cx == 256);
			CHECK(viewport.cy == 144);
			gs_stage_texture(readback.get(), destination.get());
			uint8_t *mapped = nullptr;
			uint32_t pitch = 0;
			REQUIRE(gs_stagesurface_map(readback.get(), &mapped, &pitch));
			if (frame == 1) {
				bool opaque = true;
				std::uint64_t colorSum = 0;
				for (size_t y = 0; y < 144; ++y)
					for (size_t x = 0; x < 256; ++x) {
						const auto *pixel = mapped + y * pitch + x * 4;
						opaque &= pixel[3] == 255;
						colorSum += pixel[0] + pixel[1] + pixel[2];
					}
				CHECK(opaque);
				CHECK(colorSum > 0);
			}
			if (frame != 1) {
				const auto expectedMask = refinedMask(heldExpected);
				double maxError = 0;
				for (size_t i = 0; i < expected.size(); ++i) {
					const auto expectedAlpha = 255 - expectedMask[i];
					maxError = std::max(
						maxError,
						std::abs(double(mapped[(i / 256) * pitch + (i % 256) * 4 + 3]) -
							 expectedAlpha));
				}
				CHECK(maxError <= 2);
			}
			std::vector<uint8_t> firstOutput(256 * 144 * 4);
			for (size_t y = 0; y < 144; ++y)
				std::memcpy(firstOutput.data() + y * 256 * 4, mapped + y * pitch, 256 * 4);
			gs_stagesurface_unmap(readback.get());

			// Settings arriving mid-tick apply on the next tick, without repeating refinement.
			property.refineMaskEnabled = !property.refineMaskEnabled;
			property.blurBackgroundEnabled = !property.blurBackgroundEnabled;
			pipeline.update(property);
			const auto captured = inputData->renderCount_;
			for (int draw = 0; draw < 3; ++draw) {
				obs_source_video_render(inputSource.get());
				CHECK(inputData->renderCount_ == captured + draw + 1);
				CHECK(gs_get_render_target() == destination.get());
				CHECK(gs_get_color_space() == GS_CS_709_EXTENDED);
				gs_stage_texture(readback.get(), destination.get());
				REQUIRE(gs_stagesurface_map(readback.get(), &mapped, &pitch));
				for (size_t y = 0; y < 144; ++y)
					CHECK(std::memcmp(firstOutput.data() + y * 256 * 4, mapped + y * pitch,
							  256 * 4) == 0);
				gs_stagesurface_unmap(readback.get());
			}
			property.refineMaskEnabled = !property.refineMaskEnabled;
			property.blurBackgroundEnabled = !property.blurBackgroundEnabled;
			pipeline.update(property);
		}
		std::unique_ptr<obs_data_t, void (*)(obs_data_t *)> calibrated{obs_source_get_settings(filter.get()),
									       obs_data_release};
		CHECK_FALSE(obs_data_has_user_value(calibrated.get(), "v2_Model_MatteSensitivity"));
		REQUIRE(pipeline.autoSetSensitivity());
		REQUIRE(obs_data_has_user_value(calibrated.get(), "v2_Model_MatteSensitivity"));
		const double sensitivity = obs_data_get_double(calibrated.get(), "v2_Model_MatteSensitivity");
		CHECK(sensitivity >= 0.0);
		CHECK(sensitivity <= 100.0);
		// Seed a portrait mask, then make the Engine expect RGBA while Pipeline uses BGRA.
		property.refineMaskEnabled = false;
		property.blurBackgroundEnabled = false;
		pipeline.update(property);
		auto retainedInput = portraitInput();
		std::vector<uint8_t> retainedPixels(256 * 144 * 4, 255);
		for (size_t i = 0; i < 256 * 144; ++i)
			for (size_t c = 0; c < 3; ++c)
				retainedPixels[i * 4 + 2 - c] =
					uint8_t(std::round(retainedInput[c * 256 * 144 + i] * 255));
		const auto *retainedBytes = retainedPixels.data();
		std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> portrait{
			gs_texture_create(256, 144, GS_BGRA, 1, &retainedBytes, 0), gs_texture_destroy};
		REQUIRE(portrait);
		inputData->image_ = portrait.get();
		pipeline.videoTick(1.0F / 30);
		obs_source_video_render(inputSource.get());
		using BackgroundRemoval::MPLD10PipelineTestAccess;
		std::unique_ptr<gs_stagesurf_t, void (*)(gs_stagesurf_t *)> maskReadback{
			gs_stagesurface_create(256, 144, GS_R32F), gs_stagesurface_destroy};
		REQUIRE(maskReadback);
		auto readMask = [&] {
			gs_stage_texture(maskReadback.get(), MPLD10PipelineTestAccess::alphaTexture(pipeline));
			uint8_t *mapped = nullptr;
			uint32_t pitch = 0;
			REQUIRE(gs_stagesurface_map(maskReadback.get(), &mapped, &pitch));
			std::vector<float> result(256 * 144);
			for (size_t y = 0; y < 144; ++y)
				std::memcpy(result.data() + y * 256, mapped + y * pitch, 256 * sizeof(float));
			gs_stagesurface_unmap(maskReadback.get());
			return result;
		};
		// Bypassing the finite-threshold gate must still populate this frame's input.
		auto ungatedPixels = retainedPixels;
		for (size_t i = 0; i < 256 * 144; ++i)
			for (size_t c = 0; c < 3; ++c)
				ungatedPixels[i * 4 + c] = 255 - ungatedPixels[i * 4 + c];
		const auto *ungatedBytes = ungatedPixels.data();
		std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> ungated{
			gs_texture_create(256, 144, GS_BGRA, 1, &ungatedBytes, 0), gs_texture_destroy};
		REQUIRE(ungated);
		inputData->image_ = ungated.get();
		property.imageSimilarityThreshold = std::numeric_limits<double>::quiet_NaN();
		pipeline.update(property);
		pipeline.videoTick(1.0F / 30);
		obs_source_video_render(inputSource.get());
		pipeline.videoTick(1.0F / 30);
		obs_source_video_render(inputSource.get());
		gs_stage_texture(readback.get(), MPLD10PipelineTestAccess::lastInputTexture(pipeline));
		uint8_t *ungatedMapped = nullptr;
		uint32_t ungatedPitch = 0;
		REQUIRE(gs_stagesurface_map(readback.get(), &ungatedMapped, &ungatedPitch));
		for (size_t y = 0; y < 144; ++y)
			CHECK(std::memcmp(ungatedMapped + y * ungatedPitch, ungatedPixels.data() + y * 256 * 4,
					  256 * 4) == 0);
		gs_stagesurface_unmap(readback.get());
		property.imageSimilarityThreshold = 40;
		pipeline.update(property);
		inputData->image_ = portrait.get();
		pipeline.videoTick(1.0F / 30);
		obs_source_video_render(inputSource.get());
		const auto heldMask = readMask();
		CHECK(*std::max_element(heldMask.begin(), heldMask.end()) >
		      *std::min_element(heldMask.begin(), heldMask.end()));
		auto *heldReference = MPLD10PipelineTestAccess::lastInputTexture(pipeline);
		std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> rgba{
			gs_texture_create(256, 144, GS_RGBA, 1, &retainedBytes, 0), gs_texture_destroy};
		REQUIRE(rgba);
		auto differentFormatEngine = std::make_unique<BackgroundRemoval::MPLD10Engine>(
			static_cast<ID3D11Device *>(gs_get_device_obj()),
			BackgroundRemoval::MPLD10Engine::Output::GpuMask);
		REQUIRE(differentFormatEngine->runAsync(
				static_cast<ID3D11Texture2D *>(gs_texture_get_obj(rgba.get()))) == S_OK);
		REQUIRE(differentFormatEngine->ensureOutput() == S_OK);
		MPLD10PipelineTestAccess::replaceEngine(pipeline, std::move(differentFormatEngine));
		std::vector<uint8_t> whitePixels(256 * 144 * 4, 255);
		const auto *whiteBytes = whitePixels.data();
		std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> white{
			gs_texture_create(256, 144, GS_BGRA, 1, &whiteBytes, 0), gs_texture_destroy};
		REQUIRE(white);
		inputData->image_ = white.get();
		property.refineMaskEnabled = true;
		property.refineMaskThreshold = 0;
		pipeline.update(property);
		pipeline.videoTick(1.0F / 30);
		const auto beforeRecreation = inputData->renderCount_;
		const auto remainingBeforeFailure = MPLD10PipelineTestAccess::remaining(pipeline);
		obs_source_video_render(inputSource.get());
		CHECK(pipeline.needsRecreation());
		CHECK(MPLD10PipelineTestAccess::remaining(pipeline) == remainingBeforeFailure);
		CHECK(inputData->renderCount_ == beforeRecreation + 1);
		CHECK(MPLD10PipelineTestAccess::lastInputTexture(pipeline) != heldReference);
		CHECK(readMask() == heldMask);
		heldReference = MPLD10PipelineTestAccess::lastInputTexture(pipeline);
		gs_stage_texture(readback.get(), destination.get());
		uint8_t *heldOutput = nullptr;
		uint32_t heldPitch = 0;
		REQUIRE(gs_stagesurface_map(readback.get(), &heldOutput, &heldPitch));
		int reuseError = 0;
		for (size_t y = 0; y < 144; ++y)
			for (size_t x = 0; x < 256; ++x)
				for (size_t c = 0; c < 4; ++c)
					reuseError = std::max(reuseError,
							      std::abs(int(heldOutput[y * heldPitch + x * 4 + c]) -
								       (heldMask[y * 256 + x] > 0.5F ? 0 : 255)));
		gs_stagesurface_unmap(readback.get());
		CHECK(reuseError <= 1);
		obs_source_video_render(inputSource.get());
		CHECK(inputData->renderCount_ == beforeRecreation + 2);
		CHECK(readMask() == heldMask);
		// If the parent has not replaced it yet, another tick must not resume inference.
		pipeline.videoTick(1.0F / 30);
		obs_source_video_render(inputSource.get());
		CHECK(inputData->renderCount_ == beforeRecreation + 3);
		CHECK(pipeline.needsRecreation());
		CHECK(readMask() == heldMask);
		CHECK(MPLD10PipelineTestAccess::lastInputTexture(pipeline) == heldReference);
		CHECK(gs_get_render_target() == destination.get());

		// Early return must preserve the caller's render target as well.
		obs_source_set_enabled(filter.get(), false);
		pipeline.videoRender(nullptr);
		CHECK(gs_get_render_target() == destination.get());
		obs_source_set_enabled(filter.get(), true);
	}
	// One-frame delayed motion starts a burst of 1 + ceil(FPS/30) updates.
	// Explicit draining belongs to this test only, so completion timing is deterministic.
	for (const std::uint32_t additional : {1u, 2u, 4u}) {
		using Access = BackgroundRemoval::MPLD10PipelineTestAccess;
		property.imageSimilarityThreshold = 40;
		property.refineMaskEnabled = false;
		property.blurBackgroundEnabled = false;
		BackgroundRemoval::MPLD10Pipeline pipeline(property, filter.get(), 256, 144);
		pipeline.additionalInferenceFrames_.store(additional, std::memory_order_relaxed);
		filterData->pipeline_ = &pipeline;
		inputData->pipeline_ = &pipeline;
		inputData->image_ = image.get();
		auto *initialReference = Access::lastInputTexture(pipeline);
		pipeline.videoTick(1.0F / 30);
		obs_source_video_render(inputSource.get());
		CHECK(Access::lastInputTexture(pipeline) != initialReference);
		CHECK(Access::remaining(pipeline) == 0);
		CHECK(Access::staged(pipeline));
		// A failed next-tick Map must not permit input/settings reuse.
		auto heldStaging = std::move(Access::staging(pipeline));
		auto *heldInput = Access::lastInputTexture(pipeline);
		pipeline.videoTick(1.0F / 30);
		CHECK(Access::staged(pipeline));
		obs_source_video_render(inputSource.get());
		CHECK(Access::lastInputTexture(pipeline) == heldInput);
		CHECK(Access::remaining(pipeline) == 0);
		Access::staging(pipeline) = std::move(heldStaging);
		for (std::uint32_t frame = 0; frame <= additional; ++frame) {
			pipeline.videoTick(1.0F / 30);
			auto *reference = Access::lastInputTexture(pipeline);
			obs_source_video_render(inputSource.get());
			CHECK(Access::lastInputTexture(pipeline) != reference);
			CHECK(Access::remaining(pipeline) == additional - frame);
			pipeline.additionalInferenceFrames_.store(additional + 1, std::memory_order_relaxed);
			CHECK(Access::remaining(pipeline) == additional - frame);
			pipeline.additionalInferenceFrames_.store(additional, std::memory_order_relaxed);
			reference = Access::lastInputTexture(pipeline);
			obs_source_video_render(inputSource.get());
			CHECK(Access::lastInputTexture(pipeline) == reference);
			CHECK(Access::remaining(pipeline) == additional - frame);
		}
		pipeline.videoTick(1.0F / 30);
		auto *reference = Access::lastInputTexture(pipeline);
		obs_source_video_render(inputSource.get());
		CHECK(Access::lastInputTexture(pipeline) != reference);
		CHECK(Access::remaining(pipeline) == 0);
		CHECK_FALSE(pipeline.needsRecreation());
	}
	{
		using Access = BackgroundRemoval::MPLD10PipelineTestAccess;
		BackgroundRemoval::MPLD10Pipeline pipeline(property, filter.get(), 256, 144);
		pipeline.additionalInferenceFrames_.store(2, std::memory_order_relaxed);
		for (const auto [value, sensitivity, motion] :
		     std::array{std::tuple{0u, 100.0F, false}, std::tuple{255u, 0.0F, true},
				std::tuple{1u, 48.0F, false}, std::tuple{1u, 48.3F, true}}) {
			std::vector<std::uint8_t> bytes(256 * 144, static_cast<std::uint8_t>(value));
			const auto *data = bytes.data();
			std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> difference{
				gs_texture_create(256, 144, GS_R8, 1, &data, 0), gs_texture_destroy};
			REQUIRE(difference);
			pipeline.imageSimilarityThreshold_.store(sensitivity, std::memory_order_relaxed);
			Access::setRemaining(pipeline, 0);
			Access::stageDifference(pipeline, difference.get());
			CHECK(Access::remaining(pipeline) == 0);
			pipeline.videoTick(1.0F / 60);
			CHECK_FALSE(Access::staged(pipeline));
			CHECK(Access::remaining(pipeline) == (motion ? 3u : 0u));
			Access::setRemaining(pipeline, 1);
			pipeline.videoTick(1.0F / 60);
			CHECK(Access::remaining(pipeline) == 1);
			// A new ON result extends an existing burst.
			Access::stageDifference(pipeline, difference.get());
			pipeline.videoTick(1.0F / 60);
			CHECK(Access::remaining(pipeline) == (motion ? 3u : 1u));
		}
		// Failed Map retains the unread result and burst until a later successful Map.
		Access::invalidateStaging(pipeline);
		for (const std::uint32_t remaining : {0u, 4u}) {
			Access::setRemaining(pipeline, remaining);
			// Inject an unread result whose staging resource has become unavailable.
			Access::stageDifference(pipeline, nullptr);
			pipeline.videoTick(1.0F / 30);
			CHECK(Access::staged(pipeline));
			CHECK(Access::remaining(pipeline) == remaining);
		}
	}
	gs_set_render_target_with_color_space(previousTarget, nullptr, previousSpace);

	CHECK(gs_get_render_target() == previousTarget);
	if (validationDevice)
		GpuAccessValidation::checkMessages(validationDevice.Get());
}

#endif

int testMain(int argc, char **argv)
{
	GpuAccessValidation::enable();
	// Enable the standard validation layer when installed. Timing is measured
	// separately with --benchmark-no-debug to avoid instrumentation overhead.
	bool debug = true;
	std::vector<char *> arguments{argv[0]};
	for (int i = 1; i < argc; ++i) {
		if (std::string(argv[i]) == "--benchmark-no-debug")
			debug = false;
		else
			arguments.push_back(argv[i]);
	}
	ComPtr<ID3D12Debug> validation;
	if (debug && SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&validation))))
		validation->EnableDebugLayer();
#ifdef GPU_LANDSCAPE_MODEL
	winrt::init_apartment(winrt::apartment_type::single_threaded);
#endif
	doctest::Context context(static_cast<int>(arguments.size()), arguments.data());
	const int result = context.run();
#ifdef GPU_LANDSCAPE_MODEL
	winrt::uninit_apartment();
#endif
	return result;
}

int main(int argc, char **argv)
{
	if (TraceLoggingRegister(g_hProvider) != ERROR_SUCCESS)
		return 1;
	try {
		const int result = testMain(argc, argv);
		TraceLoggingUnregister(g_hProvider);
		return result;
	} catch (...) {
		TraceLoggingUnregister(g_hProvider);
		throw;
	}
}
