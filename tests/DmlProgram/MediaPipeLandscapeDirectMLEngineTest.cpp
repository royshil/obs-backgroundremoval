#include "TraceLoggingProvider.hpp"
// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
// SPDX-License-Identifier: GPL-3.0-or-later
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>
#include "BackgroundRemoval/windows/MediaPipeLandscapeDirectMLEngine.hpp"
#include <graphics/graphics.h>
#include <d3d11_4.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include "DmlProgram/BaselineDmlProgramRuntime.hpp"
#include <winrt/base.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <vector>
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
	BackgroundRemoval::MediaPipeLandscapeDirectMLEngine engine;
	auto reference = BackgroundRemoval::Generated::makeMediaPipeLandscapeDmlProgram();
	const auto portrait = portraitInput();
	std::vector<float> output(256 * 144), expected(output.size());
	CHECK_FALSE(engine.isDone());
	CHECK_THROWS_AS(engine.getResult(output), std::logic_error);
	CHECK_THROWS_AS(engine.blockCpuUntilInferenceCompletes(), std::logic_error);

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
	BackgroundRemoval::MediaPipeLandscapeDirectMLEngine rgbaEngine;
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
			CHECK(frameEngine.runAsync(image.get()) ==
			      BackgroundRemoval::MediaPipeLandscapeDirectMLEngine::RunResult::Submitted);
			CHECK_THROWS_AS(frameEngine.runAsync(image.get()), std::logic_error);
			if (frame == 0 && format == GS_BGRA) {
				CHECK_FALSE(frameEngine.isDone());
				CHECK_THROWS_AS(frameEngine.getResult(output), std::logic_error);
				winrt::check_hresult(gate->Signal(1));
			}
			frameEngine.blockCpuUntilInferenceCompletes();
			CHECK(frameEngine.isDone());
			CHECK_THROWS_AS(frameEngine.getResult(std::span(output).first(1)), std::invalid_argument);
			frameEngine.getResult(output);
			CHECK_FALSE(frameEngine.isDone());
			CHECK_THROWS_AS(frameEngine.getResult(output), std::logic_error);
			const std::array inputs{std::as_bytes(std::span(input))};
			reference->run(inputs, std::as_writable_bytes(std::span(expected)));
			double maxError = 0;
			for (size_t i = 0; i < output.size(); ++i) {
				REQUIRE(std::isfinite(output[i]));
				maxError = std::max(maxError, std::abs(double(output[i]) - expected[i]));
			}
			CAPTURE(frame);
			CHECK(maxError < 2e-5);
		}
	}
	const std::uint8_t pixel[4]{0, 0, 0, 255};
	const auto *smallBytes = pixel;
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> smallImage{
		gs_texture_create(1, 1, GS_BGRA, 1, &smallBytes, 0), gs_texture_destroy};
	REQUIRE(smallImage);
	CHECK_THROWS_AS(engine.runAsync(smallImage.get()), std::invalid_argument);
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> wrongFormat{
		gs_texture_create(256, 144, GS_R32F, 1, nullptr, 0), gs_texture_destroy};
	REQUIRE(wrongFormat);
	CHECK(engine.runAsync(wrongFormat.get()) ==
	      BackgroundRemoval::MediaPipeLandscapeDirectMLEngine::RunResult::NeedsRecreation);
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> changedFormat{
		gs_texture_create(256, 144, GS_RGBA, 1, nullptr, 0), gs_texture_destroy};
	REQUIRE(changedFormat);
	CHECK(engine.runAsync(changedFormat.get()) ==
	      BackgroundRemoval::MediaPipeLandscapeDirectMLEngine::RunResult::NeedsRecreation);
	{
		// The owner replaces the Engine, rather than the Engine rebuilding itself.
		auto replacement = std::make_unique<BackgroundRemoval::MediaPipeLandscapeDirectMLEngine>();
		CHECK(replacement->runAsync(changedFormat.get()) ==
		      BackgroundRemoval::MediaPipeLandscapeDirectMLEngine::RunResult::Submitted);
		replacement->blockCpuUntilInferenceCompletes();
		replacement->getResult(output);
	}
	CHECK_FALSE(engine.isDone());
	CHECK_THROWS_AS(engine.runAsync(nullptr), std::invalid_argument);
	// Destruction must drain a submitted result even when the caller never reads it.
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> unconsumedImage{
		gs_texture_create(256, 144, GS_BGRA, 1, nullptr, 0), gs_texture_destroy};
	REQUIRE(unconsumedImage);
	// A format-contract rejection must leave the recorded commands reusable.
	engine.runAsync(unconsumedImage.get());
	engine.blockCpuUntilInferenceCompletes();
	engine.getResult(output);
	CHECK(rgbaEngine.runAsync(unconsumedImage.get()) ==
	      BackgroundRemoval::MediaPipeLandscapeDirectMLEngine::RunResult::NeedsRecreation);
	{
		BackgroundRemoval::MediaPipeLandscapeDirectMLEngine initiallyInvalid;
		CHECK_THROWS_AS(initiallyInvalid.runAsync(wrongFormat.get()), std::invalid_argument);
		initiallyInvalid.runAsync(unconsumedImage.get());
		initiallyInvalid.blockCpuUntilInferenceCompletes();
		initiallyInvalid.getResult(output);
	}
	{
		BackgroundRemoval::MediaPipeLandscapeDirectMLEngine unconsumed;
		unconsumed.runAsync(unconsumedImage.get());
	}
}

int testMain(int argc, char **argv)
{
	doctest::Context context(argc, argv);
	return context.run();
}

int main(int argc, char **argv)
{
	if (TraceLoggingRegister(provider) != ERROR_SUCCESS)
		return 1;
	try {
		const int result = testMain(argc, argv);
		TraceLoggingUnregister(provider);
		return result;
	} catch (...) {
		TraceLoggingUnregister(provider);
		throw;
	}
}
