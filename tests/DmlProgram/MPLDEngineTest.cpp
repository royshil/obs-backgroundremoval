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
#include "BackgroundRemoval/windows/MediaPipeLandscapeDirectML/MPLDEngine.hpp"
#include <graphics/graphics.h>
#include <d3d11_4.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include "DmlProgram/BaselineDmlProgramRuntime.hpp"
#include <winrt/base.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <limits>
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

namespace BackgroundRemoval::Generated {
auto makeMediaPipeLandscapeDmlProgram() -> std::unique_ptr<BaselineDml::BaselineDmlProgramRuntime>;
}
using Microsoft::WRL::ComPtr;
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

auto completed(BackgroundRemoval::MPLDEngine &engine) -> bool
{
	bool done = false;
	winrt::check_hresult(engine.isDone(done));
	return done;
}

TEST_CASE("LandscapeEngineBgraInput")
{
	winrt::init_apartment();
	struct Apartment {
		~Apartment() { winrt::uninit_apartment(); }
	} apartment;
	graphics_t *graphics = nullptr;
	REQUIRE(gs_create(&graphics, OBS_TEST_BIN_DIR "/libobs-d3d11.dll", 0) == 0);
	struct GraphicsCleanup {
		graphics_t *g;
		~GraphicsCleanup()
		{
			gs_leave_context();
			gs_destroy(g);
		}
	} cleanup{graphics};
	gs_enter_context(graphics);
	BackgroundRemoval::MPLDEngine engine;
	auto reference = BackgroundRemoval::Generated::makeMediaPipeLandscapeDmlProgram();
	const auto portrait = portraitInput();
	std::vector<float> output(256 * 144), expected(output.size());
	CHECK_FALSE(engine.needsRecreation());
	CHECK_FALSE(completed(engine));
	CHECK(engine.getResult(output) == E_PENDING);
	CHECK(engine.blockCpuUntilInferenceCompletes() == E_UNEXPECTED);

	// Hold the producer queue so unfinished-result checks are deterministic.
	ComPtr<ID3D11Device5> producer;
	winrt::check_hresult(static_cast<ID3D11Device *>(gs_get_device_obj())->QueryInterface(IID_PPV_ARGS(&producer)));
	ComPtr<IDXGIDevice> dxgi;
	winrt::check_hresult(producer.As(&dxgi));
	ComPtr<IDXGIAdapter> adapter;
	winrt::check_hresult(dxgi->GetAdapter(&adapter));
	ComPtr<ID3D12Device> gateDevice;
	winrt::check_hresult(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&gateDevice)));
	ComPtr<ID3D12Fence> gate;
	winrt::check_hresult(gateDevice->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&gate)));
	HANDLE gateHandle = nullptr;
	winrt::check_hresult(gateDevice->CreateSharedHandle(gate.Get(), nullptr, GENERIC_ALL, nullptr, &gateHandle));
	std::unique_ptr<void, decltype([](void *h) { CloseHandle(h); })> handle{gateHandle};
	ComPtr<ID3D11Fence> gate11;
	winrt::check_hresult(producer->OpenSharedFence(gateHandle, IID_PPV_ARGS(&gate11)));
	ComPtr<ID3D11DeviceContext> nativeContext;
	producer->GetImmediateContext(&nativeContext);
	ComPtr<ID3D11DeviceContext4> context;
	winrt::check_hresult(nativeContext.As(&context));
	BackgroundRemoval::MPLDEngine rgbaEngine;
	for (const auto format : {GS_BGRA, GS_RGBA}) {
		auto &frameEngine = format == GS_BGRA ? engine : rgbaEngine;
		for (int frame = 0; frame < 5; ++frame) {
			auto input = portrait;
			if (frame == 1)
				for (auto &v : input)
					v = 1 - v;
			std::vector<uint8_t> pixels(256 * 144 * 4);
			for (size_t i = 0; i < 256 * 144; ++i) {
				for (size_t c = 0; c < 3; ++c)
					pixels[4 * i + (format == GS_BGRA ? 2 - c : c)] =
						uint8_t(std::round(input[c * 256 * 144 + i] * 255));
				pixels[4 * i + 3] = uint8_t(i % 256);
			}
			const auto *bytes = pixels.data();
			std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> image{
				gs_texture_create(256, 144, format, 1, &bytes, 0), gs_texture_destroy};
			REQUIRE(image);
			if (frame == 0 && format == GS_BGRA) {
				winrt::check_hresult(context->Wait(gate11.Get(), 1));
			}
			struct ReleaseGate {
				ID3D12Fence *fence;
				~ReleaseGate() { fence->Signal(1); }
			} releaseGate{gate.Get()};
			CHECK(frameEngine.runAsync(image.get()) == S_OK);
			CHECK(frameEngine.runAsync(image.get()) == E_PENDING);
			CHECK_FALSE(frameEngine.needsRecreation());
			if (frame == 0 && format == GS_BGRA) {
				CHECK_FALSE(completed(frameEngine));
				CHECK(frameEngine.getResult(output) == E_PENDING);
				winrt::check_hresult(gate->Signal(1));
			}
			CHECK(frameEngine.blockCpuUntilInferenceCompletes() == S_OK);
			CHECK(completed(frameEngine));
			CHECK(frameEngine.getResult(std::span(output).first(1)) == E_INVALIDARG);
			CHECK(frameEngine.getResult(output) == S_OK);
			CHECK_FALSE(completed(frameEngine));
			CHECK(frameEngine.getResult(output) == E_PENDING);
			const std::array inputs{std::as_bytes(std::span(input))};
			runLandscapeReference(*reference, input, expected);
			double maxError = 0;
			for (size_t i = 0; i < output.size(); ++i) {
				REQUIRE(std::isfinite(output[i]));
				maxError = std::max(maxError, std::abs(double(output[i]) - expected[i]));
			}
			CAPTURE(frame);
			CHECK(maxError < 2e-5);
			if (frame == 0 && format == GS_BGRA) {
				BackgroundRemoval::MPLDEngine gpuEngine(
					BackgroundRemoval::MPLDEngine::Output::GpuMask);
				CHECK(engine.gpuOutput() == nullptr);
				CHECK(gpuEngine.complete() == E_PENDING);
				winrt::check_hresult(context->Wait(gate11.Get(), 2));
				struct ReleaseMaskGate {
					ID3D12Fence *fence;
					~ReleaseMaskGate() { fence->Signal(2); }
				} releaseMaskGate{gate.Get()};
				CHECK(gpuEngine.runAsync(image.get()) == S_OK);
				CHECK(gpuEngine.getResult(output) == E_UNEXPECTED);
				CHECK_FALSE(completed(gpuEngine));
				CHECK(gpuEngine.complete() == E_PENDING);
				winrt::check_hresult(gate->Signal(2));

				CHECK(gpuEngine.blockCpuUntilSubmittedGpuWorkCompletes() == S_OK);
				REQUIRE(completed(gpuEngine));
				CHECK(gpuEngine.complete() == S_OK);
				CHECK_FALSE(completed(gpuEngine));
				std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> mask{
					gs_texture_wrap_obj(gpuEngine.maskTexture()), gs_texture_destroy};
				REQUIRE(mask);
				std::unique_ptr<gs_stagesurf_t, void (*)(gs_stagesurf_t *)> staging{
					gs_stagesurface_create(256, 144, GS_R8), gs_stagesurface_destroy};
				REQUIRE(staging);
				gs_stage_texture(staging.get(), mask.get());
				std::uint8_t *data = nullptr;
				std::uint32_t stride = 0;
				REQUIRE(gs_stagesurface_map(staging.get(), &data, &stride));
				int maximum = 0;
				for (size_t y = 0; y < 144; ++y)
					for (size_t x = 0; x < 256; ++x)
						maximum = std::max(
							maximum,
							std::abs(int(data[y * stride + x]) -
								 int(std::lround(std::clamp(1.f - expected[y * 256 + x],
											    0.f, 1.f) *
										 255))));
				gs_stagesurface_unmap(staging.get());
				CHECK(maximum <= 1);
				CHECK(gpuEngine.complete() == E_PENDING);
				// Leave inference + U8 conversion pending for destruction.
				CHECK(gpuEngine.runAsync(image.get()) == S_OK);
			}
		}
	}
	const std::uint8_t pixel[4]{0, 0, 0, 255};
	const auto *smallBytes = pixel;
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> smallImage{
		gs_texture_create(1, 1, GS_BGRA, 1, &smallBytes, 0), gs_texture_destroy};
	REQUIRE(smallImage);
	CHECK(engine.runAsync(smallImage.get()) == E_INVALIDARG);
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> wrongFormat{
		gs_texture_create(256, 144, GS_R32F, 1, nullptr, 0), gs_texture_destroy};
	REQUIRE(wrongFormat);
	CHECK(engine.runAsync(wrongFormat.get()) == E_INVALIDARG);
	CHECK(engine.needsRecreation());
	CHECK_FALSE(completed(engine));
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> changedFormat{
		gs_texture_create(256, 144, GS_RGBA, 1, nullptr, 0), gs_texture_destroy};
	REQUIRE(changedFormat);
	CHECK(engine.runAsync(changedFormat.get()) == E_INVALIDARG);
	CHECK(engine.needsRecreation());
	{
		// The owner replaces the Engine, rather than the Engine rebuilding itself.
		auto replacement = std::make_unique<BackgroundRemoval::MPLDEngine>();
		CHECK(replacement->runAsync(changedFormat.get()) == S_OK);
		CHECK(replacement->blockCpuUntilInferenceCompletes() == S_OK);
		CHECK(replacement->getResult(output) == S_OK);
	}
	CHECK_FALSE(completed(engine));
	CHECK(engine.runAsync(nullptr) == E_INVALIDARG);
	CHECK_FALSE(engine.needsRecreation());
	// Destruction must drain a submitted result even when the caller never reads it.
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> unconsumedImage{
		gs_texture_create(256, 144, GS_BGRA, 1, nullptr, 0), gs_texture_destroy};
	REQUIRE(unconsumedImage);
	// A format-contract rejection must leave the recorded commands reusable.
	CHECK(engine.runAsync(unconsumedImage.get()) == S_OK);
	CHECK_FALSE(engine.needsRecreation());
	CHECK(engine.blockCpuUntilInferenceCompletes() == S_OK);
	CHECK(engine.getResult(output) == S_OK);
	CHECK(rgbaEngine.runAsync(unconsumedImage.get()) == E_INVALIDARG);
	{
		BackgroundRemoval::MPLDEngine initiallyInvalid;
		CHECK(initiallyInvalid.runAsync(wrongFormat.get()) == E_INVALIDARG);
		CHECK_FALSE(initiallyInvalid.needsRecreation());
		CHECK(initiallyInvalid.runAsync(unconsumedImage.get()) == S_OK);
		CHECK(initiallyInvalid.blockCpuUntilInferenceCompletes() == S_OK);
		CHECK(initiallyInvalid.getResult(output) == S_OK);
	}
	{
		BackgroundRemoval::MPLDEngine unconsumed;
		CHECK(unconsumed.runAsync(unconsumedImage.get()) == S_OK);
	}
}

int testMain(int argc, char **argv)
{
	doctest::Context context;
	context.applyCommandLine(argc, argv);
	return context.run();
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
