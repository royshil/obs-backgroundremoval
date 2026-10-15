#include "TraceLoggingProvider.hpp"
// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
// SPDX-License-Identifier: GPL-3.0-or-later
#include "BackgroundRemoval/windows/MediaPipeLandscapeDirectMLEngine.hpp"
#include "DmlProgram/BaselineDmlProgramRuntime.hpp"
#include "BackgroundRemoval/Operators.hpp"
#include <graphics/graphics.h>
#include <winrt/base.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <numeric>
#include <vector>
#include <DirectXPackedVector.h>

void runLandscapeReference(BaselineDml::BaselineDmlProgramRuntime &runtime, std::span<const float> input, std::span<float> output)
{
    if (runtime.inputInfo(0).type != BaselineDml::ProgramDefinition::dtFloat32 ||
        runtime.outputInfo().type != BaselineDml::ProgramDefinition::dtFloat32) {
        throw std::runtime_error("Landscape requires FP32 input and output");
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

int testMain()
try {
	winrt::init_apartment();
	graphics_t *graphics = nullptr;
	if (gs_create(&graphics, OBS_TEST_BIN_DIR "/libobs-d3d11.dll", 0) != 0)
		throw std::runtime_error("OBS graphics creation");
	struct GraphicsCleanup {
		graphics_t *g;
		~GraphicsCleanup()
		{
			gs_leave_context();
			gs_destroy(g);
		}
	} cleanup{graphics};
	gs_enter_context(graphics);
	auto runtime = BackgroundRemoval::Generated::makeMediaPipeLandscapeDmlProgram();
	BackgroundRemoval::MediaPipeLandscapeDirectMLEngine engine;
	Operators::ResizeU8 resize;
	std::vector<uint8_t> modelMask(256 * 144), mask(1280 * 720);
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> sourceDraw{
		gs_texture_create(1280, 720, GS_BGRA, 1, nullptr, GS_RENDER_TARGET), gs_texture_destroy};
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> alpha{
		gs_texture_create(1280, 720, GS_R8, 1, nullptr, GS_DYNAMIC), gs_texture_destroy};
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> finalDraw{
		gs_texture_create(1280, 720, GS_BGRA, 1, nullptr, GS_RENDER_TARGET), gs_texture_destroy};
	if (!sourceDraw || !alpha || !finalDraw)
		throw std::runtime_error("Benchmark output resources");
	auto input = portraitInput();
	auto image = [&](bool inverted) {
		std::vector<uint8_t> data(256 * 144 * 4);
		for (size_t i = 0; i < 256 * 144; ++i) {
			for (size_t c = 0; c < 3; ++c)
				data[4 * i + 2 - c] = uint8_t(std::round(
					(inverted ? 1 - input[c * 256 * 144 + i] : input[c * 256 * 144 + i]) * 255));
			data[4 * i + 3] = 255;
		}
		const auto *bytes = data.data();
		return std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)>{
			gs_texture_create(256, 144, GS_BGRA, 1, &bytes, 0), gs_texture_destroy};
	};
	auto reference = image(false), changed = image(true);
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> difference{
		gs_texture_create(256, 144, GS_R8, 1, nullptr, GS_RENDER_TARGET), gs_texture_destroy};
	std::unique_ptr<gs_stagesurf_t, void (*)(gs_stagesurf_t *)> staging{gs_stagesurface_create(256, 144, GS_R8),
									    gs_stagesurface_destroy};
	std::ifstream effectFile("landscape.effect");
	std::string shader{std::istreambuf_iterator<char>(effectFile), {}};
	std::unique_ptr<gs_effect_t, void (*)(gs_effect_t *)> effect{
		gs_effect_create(shader.c_str(), "landscape-benchmark.effect", nullptr), gs_effect_destroy};
	if (!reference || !changed || !difference || !staging || !effect)
		throw std::runtime_error("Benchmark resource creation");
	auto *ep = gs_effect_get_param_by_name(effect.get(), "image");
	auto *ep1 = gs_effect_get_param_by_name(effect.get(), "image1");
	auto invertedInput = input;
	for (auto &v : invertedInput)
		v = 1 - v;
	std::vector<float> output(256 * 144), previous(output.size());
	const uint64_t sumThreshold = uint64_t(std::ceil(255.0 * 256 * 144 * std::pow(10.0, -40.0 / 20)));
	std::ofstream csv("samples.csv");
	csv << "scenario,iteration,method,total_ms,gate_ms,inference_ms,submit_ms,independent_ms,wait_ms,result_ms,refine_ms,draw_ms,blocked\n"
	    << std::setprecision(10);
	auto duration = [](auto start) {
		return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	};
	auto stats = [](const char *scenario, const char *method, std::vector<double> v) {
		double mean = std::accumulate(v.begin(), v.end(), 0.0) / v.size();
		std::sort(v.begin(), v.end());
		std::cout << scenario << "," << method << ",mean=" << mean << ",median=" << v[v.size() / 2]
			  << ",p95=" << v[(v.size() * 95 + 99) / 100 - 1] << " ms\n";
	};
	for (bool change : {false, true}) {
		std::vector<double> totals[2], gates[2], inferences[2], submissions[2], independent[2], waits[2],
			results[2], refinements[2], draws[2];
		unsigned blockedCounts[2]{};
		std::vector<float> expected(output.size());
		const auto &values = change ? invertedInput : input;
		const std::array inputs{std::as_bytes(std::span(values))};
		runLandscapeReference(*runtime, values, expected);
		for (int iteration = -20; iteration < 100; ++iteration) {
			for (int order = 0; order < 2; ++order) {
				const int method = (iteration + order + 20) % 2;
				auto *current = change ? changed.get() : reference.get();
				auto start = std::chrono::steady_clock::now();
				gs_set_render_target_with_color_space(difference.get(), nullptr, GS_CS_709_EXTENDED);
				gs_set_viewport(0, 0, 256, 144);
				gs_ortho(0, 256, 0, 144, -100, 100);
				gs_matrix_identity();
				gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
				while (gs_effect_loop(effect.get(), "LumaAbsDiff")) {
					gs_effect_set_texture(ep, reference.get());
					gs_effect_set_texture(ep1, current);
					gs_draw_sprite(nullptr, 0, 256, 144);
				}
				gs_stage_texture(staging.get(), difference.get());
				uint8_t *data = nullptr;
				uint32_t pitch = 0;
				if (!gs_stagesurface_map(staging.get(), &data, &pitch))
					throw std::runtime_error("Difference Map");
				uint64_t sum = 0;
				for (unsigned y = 0; y < 144; ++y)
					for (unsigned x = 0; x < 256; ++x)
						sum += data[size_t(y) * pitch + x];
				gs_stagesurface_unmap(staging.get());
				bool infer = sum != 0 && sum >= sumThreshold;
				if (infer != change)
					throw std::runtime_error("Gate decision mismatch");
				double gateMs = duration(start);
				auto inferStart = std::chrono::steady_clock::now();
				double submitMs = 0, waitMs = 0, resultMs = 0, refineMs = 0;
				bool blocked = false;
				auto obtainResult = [&] {
					if (!infer)
						return;
					const auto waitStart = std::chrono::steady_clock::now();
					blocked = !engine.isDone();
					if (blocked)
						engine.blockCpuUntilInferenceCompletes();
					waitMs = duration(waitStart);
					const auto readStart = std::chrono::steady_clock::now();
					engine.getResult(output);
					resultMs = duration(readStart);
				};
				if (infer) {
					const auto submitStart = std::chrono::steady_clock::now();
					engine.runAsync(current);
					submitMs = duration(submitStart);
				}
				if (method == 0)
					obtainResult();
				const auto independentStart = std::chrono::steady_clock::now();
				gs_set_render_target_with_color_space(sourceDraw.get(), nullptr, GS_CS_709_EXTENDED);
				gs_set_viewport(0, 0, 1280, 720);
				gs_ortho(0, 1280, 0, 720, -100, 100);
				while (gs_effect_loop(effect.get(), "Draw")) {
					gs_effect_set_texture(ep, current);
					gs_draw_sprite(nullptr, 0, 1280, 720);
				}
				const double independentMs = duration(independentStart);
				if (method == 1)
					obtainResult();
				if (infer) {
					const auto refineStart = std::chrono::steady_clock::now();
					for (size_t i = 0; i < output.size(); ++i)
						modelMask[i] =
							uint8_t(std::clamp((1 - output[i]) * 255.0F, 0.0F, 255.0F));
					if (!resize.apply(modelMask.data(), {256, 144, 256}, mask.data(),
							  {1280, 720, 1280}))
						throw std::runtime_error("Benchmark mask resize");
					gs_texture_set_image(alpha.get(), mask.data(), 1280, false);
					refineMs = duration(refineStart);
				}
				const auto drawStart = std::chrono::steady_clock::now();
				gs_set_render_target_with_color_space(finalDraw.get(), nullptr, GS_CS_709_EXTENDED);
				while (gs_effect_loop(effect.get(), "DrawWithoutBlur")) {
					gs_effect_set_texture(ep, sourceDraw.get());
					gs_effect_set_texture(gs_effect_get_param_by_name(effect.get(), "alphamask"),
							      alpha.get());
					gs_draw_sprite(nullptr, 0, 1280, 720);
				}
				gs_flush();
				const double drawMs = duration(drawStart);
				double inferMs = duration(inferStart), total = duration(start);
				if (infer) {
					for (size_t i = 0; i < output.size(); ++i)
						if (!std::isfinite(output[i]) ||
						    std::abs(output[i] - expected[i]) > 2e-5)
							throw std::runtime_error("Current-frame reference mismatch");
				}

				if (change) {
					if (order == 0)
						previous = output;
					else {
						double maxError = 0;
						for (size_t i = 0; i < output.size(); ++i) {
							if (!std::isfinite(output[i]))
								throw std::runtime_error("Nonfinite model output");
							maxError = std::max(maxError,
									    std::abs(double(output[i]) - previous[i]));
						}
						if (maxError > 2e-5)
							throw std::runtime_error(
								"BGRA/CPU-input model output mismatch");
					}
				}
				if (iteration >= 0) {
					totals[method].push_back(total);
					gates[method].push_back(gateMs);
					inferences[method].push_back(inferMs);
					submissions[method].push_back(submitMs);
					independent[method].push_back(independentMs);
					waits[method].push_back(waitMs);
					results[method].push_back(resultMs);
					refinements[method].push_back(refineMs);
					draws[method].push_back(drawMs);
					blockedCounts[method] += blocked;
					csv << (change ? "infer" : "skip") << "," << iteration << ","
					    << (method ? "deferred-wait" : "immediate-wait") << "," << total << ","
					    << gateMs << "," << inferMs << "," << submitMs << "," << independentMs
					    << "," << waitMs << "," << resultMs << "," << refineMs << "," << drawMs
					    << "," << blocked << "\n";
				}
				gs_set_render_target(nullptr, nullptr);
			}
			Sleep(10);
		}
		for (int method = 0; method < 2; ++method) {
			stats(change ? "infer-total" : "skip-total", method ? "deferred-wait" : "immediate-wait",
			      totals[method]);
			stats(change ? "infer-gate" : "skip-gate", method ? "deferred-wait" : "immediate-wait",
			      gates[method]);
			if (change) {
				const auto *name = method ? "deferred-wait" : "immediate-wait";
				stats("submit", name, submissions[method]);
				stats("independent", name, independent[method]);
				stats("wait", name, waits[method]);
				stats("result", name, results[method]);
				stats("refine", name, refinements[method]);
				stats("draw", name, draws[method]);
				std::cout << name << ",blocked=" << blockedCounts[method] << "/100\n";
			}
		}
	}
	std::cout << "Model output agreement passed (max absolute error <= 2e-5).\n";
	return 0;
} catch (const std::exception &e) {
	std::cerr << e.what() << "\n";
	return 1;
}

int main()
{
	if (TraceLoggingRegister(provider) != ERROR_SUCCESS)
		return 1;
	try {
		const int result = testMain();
		TraceLoggingUnregister(provider);
		return result;
	} catch (...) {
		TraceLoggingUnregister(provider);
		throw;
	}
}
