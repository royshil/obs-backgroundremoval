#include "BackgroundRemoval/PipelineFactory.hpp"
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
// SPDX-License-Identifier: GPL-3.0-or-later
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>
#include "GpuAccessValidation.hpp"
#include "DmlProgram/BaselineDmlProgramRuntime.hpp"
#include "ObsBridgeUtils/TextureReader.hpp"
#include "BackgroundRemoval/Operators.hpp"
#include "BackgroundRemoval/MainEffect.hpp"
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <wrl/client.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <vector>
#include <DirectXPackedVector.h>

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
#include "BackgroundRemoval/windows/MediaPipeLandscapeDirectMLPipeline.hpp"
#include "BackgroundRemoval/windows/MediaPipeLandscapeDirectMLEngine.hpp"
#include <wincodec.h>
#include <winrt/base.h>
namespace BackgroundRemoval::Generated {
auto makeMediaPipeLandscapeDmlProgram() -> std::unique_ptr<BaselineDml::BaselineDmlProgramRuntime>;
}
#endif

namespace {
using Microsoft::WRL::ComPtr;
namespace PD = BaselineDml::ProgramDefinition;
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
		ObsBridgeUtils::GsUnique::drain();
		gs_leave_context();
		gs_destroy(context);
	}
};
auto texture(const std::vector<float> &values, std::uint32_t width, std::uint32_t height)
	-> ObsBridgeUtils::UniqueTexturePtr
{
	const auto *bytes = reinterpret_cast<const std::uint8_t *>(values.data());
	ObsBridgeUtils::UniqueTexturePtr result(gs_texture_create(width, height, GS_R32F, 1, &bytes, GS_RENDER_TARGET));
	if (!result)
		throw std::runtime_error("Cannot create GPU input fixture");
	return result;
}
void checkDebugErrors(ID3D12Device *device)
{
	ComPtr<ID3D12InfoQueue> info;
	if (FAILED(device->QueryInterface(IID_PPV_ARGS(&info))))
		return;
	for (UINT64 i = 0; i < info->GetNumStoredMessages(); ++i) {
		SIZE_T bytes = 0;
		info->GetMessage(i, nullptr, &bytes);
		std::vector<std::byte> data(bytes);
		auto *message = reinterpret_cast<D3D12_MESSAGE *>(data.data());
		if (SUCCEEDED(info->GetMessage(i, message, &bytes)) &&
		    message->Severity <= D3D12_MESSAGE_SEVERITY_ERROR) {
			INFO(message->pDescription);
			CHECK(message->Severity > D3D12_MESSAGE_SEVERITY_ERROR);
		}
	}
}

#ifdef GPU_LANDSCAPE_MODEL
#endif
} // namespace

TEST_CASE("MainEffectLumaAbsDiff")
{
	Graphics graphics;
	auto effect = BackgroundRemoval::MainEffect::create();
	REQUIRE(effect);
	for (const auto [w, h] : {std::pair{17U, 19U}, std::pair{256U, 144U}}) {
		CAPTURE(w);
		CAPTURE(h);
		std::vector<uint8_t> pixels(size_t(w) * h * 4, 0);
		for (size_t i = 0; i < pixels.size(); i += 4) {
			pixels[i] = 255;
			pixels[i + 3] = 255;
		}
		const auto *data = pixels.data();
		ObsBridgeUtils::UniqueTexturePtr red(gs_texture_create(w, h, GS_RGBA, 1, &data, 0));
		REQUIRE(red);
		auto current = ObsBridgeUtils::makeUniqueTexture(w, h, GS_BGRA, GS_RENDER_TARGET);
		auto reference = ObsBridgeUtils::makeUniqueTexture(w, h, GS_BGRA, GS_RENDER_TARGET);
		auto diff = ObsBridgeUtils::makeUniqueTexture(w, h, GS_R8, GS_RENDER_TARGET);
		auto staging = ObsBridgeUtils::makeUniqueStagesurf(w, h, GS_R8);
		REQUIRE(current);
		REQUIRE(reference);
		REQUIRE(diff);
		REQUIRE(staging);
		effect->copy(reference.get(), red.get());
		effect->copy(current.get(), red.get());
		auto *saved = gs_get_render_target();
		auto color = gs_get_color_space();
		auto read = [&] {
			effect->lumaAbsDiff(diff.get(), reference.get(), current.get());
			CHECK(gs_get_render_target() == saved);
			CHECK(gs_get_color_space() == color);
			gs_stage_texture(staging.get(), diff.get());
			uint8_t *mapped = nullptr;
			uint32_t pitch = 0;
			if (!gs_stagesurface_map(staging.get(), &mapped, &pitch))
				throw std::runtime_error("Map test luminance image");
			struct Unmap {
				gs_stagesurf_t *s;
				~Unmap() { gs_stagesurface_unmap(s); }
			} unmap{staging.get()};
			REQUIRE(mapped);
			REQUIRE(pitch >= w);
			uint64_t total = 0;
			for (unsigned y = 0; y < h; ++y)
				for (unsigned x = 0; x < w; ++x)
					total += mapped[size_t(y) * pitch + x];
			return double(total) / (255.0 * w * h);
		};
		CHECK(read() == 0);
		for (size_t i = 0; i < pixels.size(); i += 4) {
			pixels[i] = 0;
			pixels[i + 1] = 255;
		}
		data = pixels.data();
		ObsBridgeUtils::UniqueTexturePtr green(gs_texture_create(w, h, GS_RGBA, 1, &data, 0));
		REQUIRE(green);
		effect->copy(current.get(), green.get());
		CHECK(read() == doctest::Approx(0.7152 - 0.2126).epsilon(0.01));
		// Only an accepted inference swaps the reference; repeated comparisons keep it.
		CHECK(read() == doctest::Approx(0.7152 - 0.2126).epsilon(0.01));
		std::swap(reference, current);
		effect->copy(current.get(), green.get());
		CHECK(read() == 0);
	}
}

TEST_CASE("PredicatedDirectML")
{
	const std::array<std::uint32_t, 4> shape{1, 1, 1, 4};
	const PD::InputSpecItem spec{PD::kRuntimeInput, PD::dtFloat32, 4, shape};
	const PD::Node nodes[]{PD::InputItem{PD::dtFloat32, 0}, PD::Onnx14Node{PD::Relu{PD::dtFloat32, 0, 4, shape}}};
	BaselineDml::BaselineDmlProgramRuntime runtime({1, &spec, 2, nodes, 1}, {});
	auto check = [](HRESULT hr) {
		if (FAILED(hr))
			throw std::runtime_error("Predication test HRESULT " + std::to_string(hr));
	};
	auto makeBuffer = [&](UINT64 size, D3D12_HEAP_TYPE type, D3D12_RESOURCE_STATES state) {
		D3D12_HEAP_PROPERTIES heap{};
		heap.Type = type;
		D3D12_RESOURCE_DESC desc{};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Width = size;
		desc.Height = desc.DepthOrArraySize = desc.MipLevels = desc.SampleDesc.Count = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		ComPtr<ID3D12Resource> result;
		check(runtime.device()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr,
								IID_PPV_ARGS(&result)));
		return result;
	};
	auto input = makeBuffer(16, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
	auto predicate = makeBuffer(8, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST);
	auto predicateUpload = makeBuffer(8, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
	auto result = makeBuffer(16 + sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS), D3D12_HEAP_TYPE_READBACK,
				 D3D12_RESOURCE_STATE_COPY_DEST);
	ComPtr<ID3D12CommandAllocator> allocator;
	ComPtr<ID3D12GraphicsCommandList> commands;
	ComPtr<ID3D12QueryHeap> query;
	ComPtr<ID3D12Fence> fence;
	check(runtime.device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));
	check(runtime.device()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr,
						  IID_PPV_ARGS(&commands)));
	check(commands->Close());
	D3D12_QUERY_HEAP_DESC queryDesc{D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS, 1, 0};
	check(runtime.device()->CreateQueryHeap(&queryDesc, IID_PPV_ARGS(&query)));
	check(runtime.device()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
	const float values[]{-1, 0, 0.5F, 1};
	void *mapped = nullptr;
	check(input->Map(0, nullptr, &mapped));
	std::memcpy(mapped, values, sizeof(values));
	input->Unmap(0, nullptr);
	for (const UINT64 decision : {0ULL, 1ULL}) {
		check(predicateUpload->Map(0, nullptr, &mapped));
		std::memcpy(mapped, &decision, sizeof(decision));
		predicateUpload->Unmap(0, nullptr);
		check(allocator->Reset());
		check(commands->Reset(allocator.Get(), nullptr));
		commands->CopyBufferRegion(predicate.Get(), 0, predicateUpload.Get(), 0, 8);
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition = {predicate.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
				      D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PREDICATION};
		commands->ResourceBarrier(1, &barrier);
		commands->BeginQuery(query.Get(), D3D12_QUERY_TYPE_PIPELINE_STATISTICS, 0);
		const BaselineDml::GpuInput binding{input.Get(), 0, 16};
		runtime.recordGpuRun(commands.Get(), std::span(&binding, 1), predicate.Get(), result.Get(), 0);
		commands->EndQuery(query.Get(), D3D12_QUERY_TYPE_PIPELINE_STATISTICS, 0);
		commands->ResolveQueryData(query.Get(), D3D12_QUERY_TYPE_PIPELINE_STATISTICS, 0, 1, result.Get(), 16);
		std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
		commands->ResourceBarrier(1, &barrier);
		check(commands->Close());
		ID3D12CommandList *lists[]{commands.Get()};
		runtime.queue()->ExecuteCommandLists(1, lists);
		check(runtime.queue()->Signal(fence.Get(), decision + 1));
		// Small test-only wait; production uses a completion event.
		while (fence->GetCompletedValue() < decision + 1)
			Sleep(1);
		check(runtime.device()->GetDeviceRemovedReason());
		check(result->Map(0, nullptr, &mapped));
		D3D12_QUERY_DATA_PIPELINE_STATISTICS statistics{};
		std::memcpy(&statistics, static_cast<const std::byte *>(mapped) + 16, sizeof(statistics));
		float actual[4];
		std::memcpy(actual, mapped, sizeof(actual));
		result->Unmap(0, nullptr);
		std::cout << "DirectML predicate=" << decision << " CSInvocations=" << statistics.CSInvocations << '\n';
		if (decision) {
			CHECK(statistics.CSInvocations > 0);
			CHECK(actual[0] == 0);
			CHECK(actual[2] == 0.5F);
			CHECK(actual[3] == 1);
		} else
			CHECK(statistics.CSInvocations == 0);
	}
	checkDebugErrors(runtime.device());
}

#ifdef GPU_LANDSCAPE_MODEL
namespace BackgroundRemoval {
struct MediaPipeLandscapeDirectMLPipelineTestAccess {
	static void finishGate(MediaPipeLandscapeDirectMLPipeline &pipeline)
	{
		if (pipeline.gateSubmissionPending_) {
			REQUIRE(pipeline.engine_->blockCpuUntilSubmittedGpuWorkCompletes() == S_OK);
			REQUIRE(pipeline.engine_->complete() == S_OK);
			pipeline.gateSubmissionPending_ = false;
			pipeline.gateResultAvailable_ = true;
		}
	}
	static auto remaining(const MediaPipeLandscapeDirectMLPipeline &pipeline) -> std::uint32_t
	{
		return pipeline.remainingInferenceFrames_;
	}
	static void replaceEngine(MediaPipeLandscapeDirectMLPipeline &pipeline,
				  std::unique_ptr<MediaPipeLandscapeDirectMLEngine> engine)
	{
		pipeline.engine_ = std::move(engine);
	}
	static auto alphaTexture(const MediaPipeLandscapeDirectMLPipeline &pipeline) -> gs_texture_t *
	{
		return pipeline.alphaTexture_.get();
	}
	static auto lastInputTexture(const MediaPipeLandscapeDirectMLPipeline &pipeline) -> gs_texture_t *
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
	MediaPipeLandscapeDirectMLPipeline *pipeline_ = nullptr;
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
		property.pipelineId = BackgroundRemoval::MediaPipeLandscapeDirectMLPipeline::kPipelineId;
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
		BackgroundRemoval::MediaPipeLandscapeDirectMLPipeline pipeline(property, scene, 256, 144);
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
	auto runtime = BackgroundRemoval::Generated::makeMediaPipeLandscapeDmlProgram();
	std::vector<float> expected(256 * 144);
	const std::array inputs{std::as_bytes(std::span(input))};
	runLandscapeReference(*runtime, input, expected);
	property.imageSimilarityThreshold = 40;
	{
		BackgroundRemoval::MediaPipeLandscapeDirectMLPipeline pipeline(property, filter.get(), 256, 144);
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
		BackgroundRemoval::MediaPipeLandscapeDirectMLPipelineTestAccess::finishGate(pipeline);
		inputData->image_ = image.get();
		const char *stressFrames = std::getenv("DML_TEST_STRESS_FRAMES");
		const int frameCount = stressFrames ? std::clamp(std::atoi(stressFrames), 4, 120) : 4;
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
			// Settings change must affect refinement without forcing a new inference.
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
			auto *previousReference = BackgroundRemoval::MediaPipeLandscapeDirectMLPipelineTestAccess::lastInputTexture(pipeline);
			obs_source_video_render(inputSource.get());
			if (BackgroundRemoval::MediaPipeLandscapeDirectMLPipelineTestAccess::lastInputTexture(pipeline) != previousReference)
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
				double maxError = 0;
				for (size_t i = 0; i < expected.size(); ++i) {
					const auto expectedAlpha =
						255 - uint8_t(std::clamp((1 - heldExpected[i]) * 255.0F, 0.0F, 255.0F));
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

			if (!std::getenv("DML_TICK_BENCHMARK")) {
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
			BackgroundRemoval::MediaPipeLandscapeDirectMLPipelineTestAccess::finishGate(pipeline);
		}
		std::unique_ptr<obs_data_t, void (*)(obs_data_t *)> calibrated{obs_source_get_settings(filter.get()),
									       obs_data_release};
		CHECK_FALSE(obs_data_has_user_value(calibrated.get(), "v2_Model_MatteSensitivity"));
		REQUIRE(pipeline.autoSetSensitivity());
		REQUIRE(obs_data_has_user_value(calibrated.get(), "v2_Model_MatteSensitivity"));
		const double sensitivity = obs_data_get_double(calibrated.get(), "v2_Model_MatteSensitivity");
		CHECK(sensitivity >= 0.0);
		CHECK(sensitivity <= 100.0);
		// Opt-in comparison: two renders per tick, stable input and fixed settings.
		if (std::getenv("DML_TICK_BENCHMARK")) {
			std::vector<double> first, repeated;
			for (int tick = 0; tick < 70; ++tick) {
				pipeline.videoTick(1.0F / 30);
				const auto start = std::chrono::steady_clock::now();
				obs_source_video_render(inputSource.get());
				const auto middle = std::chrono::steady_clock::now();
				obs_source_video_render(inputSource.get());
				const auto end = std::chrono::steady_clock::now();
				if (tick >= 10) {
					first.push_back(
						std::chrono::duration<double, std::milli>(middle - start).count());
					repeated.push_back(
						std::chrono::duration<double, std::milli>(end - middle).count());
				}
				// Drain outside the timing so pending final draws cannot accumulate.
				gs_stage_texture(readback.get(), destination.get());
				uint8_t *mapped = nullptr;
				uint32_t pitch = 0;
				REQUIRE(gs_stagesurface_map(readback.get(), &mapped, &pitch));
				gs_stagesurface_unmap(readback.get());
			}
			for (auto *values : {&first, &repeated}) {
				std::sort(values->begin(), values->end());
				std::cout << "TICK_BENCH,phase=" << (values == &first ? "first" : "repeat")
					  << ",n=" << values->size() << ",mean_ms="
					  << std::accumulate(values->begin(), values->end(), 0.0) / values->size()
					  << ",median_ms=" << ((*values)[29] + (*values)[30]) / 2
					  << ",p95_ms=" << (*values)[56] << '\n';
			}
		}
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
		using BackgroundRemoval::MediaPipeLandscapeDirectMLPipelineTestAccess;
		std::unique_ptr<gs_stagesurf_t, void (*)(gs_stagesurf_t *)> maskReadback{
			gs_stagesurface_create(256, 144, GS_R8), gs_stagesurface_destroy};
		REQUIRE(maskReadback);
		auto readMask = [&] {
			gs_stage_texture(maskReadback.get(),
					 MediaPipeLandscapeDirectMLPipelineTestAccess::alphaTexture(pipeline));
			uint8_t *mapped = nullptr;
			uint32_t pitch = 0;
			REQUIRE(gs_stagesurface_map(maskReadback.get(), &mapped, &pitch));
			std::vector<uint8_t> result(256 * 144);
			for (size_t y = 0; y < 144; ++y)
				std::memcpy(result.data() + y * 256, mapped + y * pitch, 256);
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
		MediaPipeLandscapeDirectMLPipelineTestAccess::finishGate(pipeline);
		pipeline.videoTick(1.0F / 30);
		obs_source_video_render(inputSource.get());
		gs_stage_texture(readback.get(),
				 MediaPipeLandscapeDirectMLPipelineTestAccess::lastInputTexture(pipeline));
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
		auto *heldReference = MediaPipeLandscapeDirectMLPipelineTestAccess::lastInputTexture(pipeline);
		std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> rgba{
			gs_texture_create(256, 144, GS_RGBA, 1, &retainedBytes, 0), gs_texture_destroy};
		REQUIRE(rgba);
		auto differentFormatEngine = std::make_unique<BackgroundRemoval::MediaPipeLandscapeDirectMLEngine>(
			BackgroundRemoval::MediaPipeLandscapeDirectMLEngine::Output::GpuMask);
		REQUIRE(differentFormatEngine->runAsync(rgba.get()) == S_OK);
		REQUIRE(differentFormatEngine->blockCpuUntilSubmittedGpuWorkCompletes() == S_OK);
		REQUIRE(differentFormatEngine->complete() == S_OK);
		MediaPipeLandscapeDirectMLPipelineTestAccess::replaceEngine(pipeline, std::move(differentFormatEngine));
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
		obs_source_video_render(inputSource.get());
		CHECK(pipeline.needsRecreation());
		CHECK(inputData->renderCount_ == beforeRecreation + 1);
		CHECK(MediaPipeLandscapeDirectMLPipelineTestAccess::lastInputTexture(pipeline) == heldReference);
		CHECK(readMask() == heldMask);
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
								       (255 - heldMask[y * 256 + x])));
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
		CHECK(MediaPipeLandscapeDirectMLPipelineTestAccess::lastInputTexture(pipeline) == heldReference);
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
		using Access = BackgroundRemoval::MediaPipeLandscapeDirectMLPipelineTestAccess;
		property.imageSimilarityThreshold = 40;
		property.refineMaskEnabled = false;
		property.blurBackgroundEnabled = false;
		BackgroundRemoval::MediaPipeLandscapeDirectMLPipeline pipeline(property, filter.get(), 256, 144);
		pipeline.additionalInferenceFrames_.store(additional, std::memory_order_relaxed);
		filterData->pipeline_ = &pipeline;
		inputData->pipeline_ = &pipeline;
		inputData->image_ = image.get();
		auto *initialReference = Access::lastInputTexture(pipeline);
		pipeline.videoTick(1.0F / 30);
		obs_source_video_render(inputSource.get());
		CHECK(Access::lastInputTexture(pipeline) == initialReference);
		Access::finishGate(pipeline);
		for (std::uint32_t frame = 0; frame <= additional; ++frame) {
			pipeline.videoTick(1.0F / 30);
			auto *reference = Access::lastInputTexture(pipeline);
			obs_source_video_render(inputSource.get());
			CHECK(Access::lastInputTexture(pipeline) != reference);
			CHECK(Access::remaining(pipeline) == additional - frame);
			reference = Access::lastInputTexture(pipeline);
			obs_source_video_render(inputSource.get());
			CHECK(Access::lastInputTexture(pipeline) == reference);
			CHECK(Access::remaining(pipeline) == additional - frame);
		}
		pipeline.videoTick(1.0F / 30);
		auto *reference = Access::lastInputTexture(pipeline);
		obs_source_video_render(inputSource.get());
		CHECK(Access::lastInputTexture(pipeline) == reference);
		CHECK_FALSE(pipeline.needsRecreation());
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
