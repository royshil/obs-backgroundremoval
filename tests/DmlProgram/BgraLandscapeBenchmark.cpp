// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <TraceLoggingProvider.h>

TRACELOGGING_DECLARE_PROVIDER(g_hProvider);
#include "BackgroundRemoval/windows/MediaPipeLandscapeDirectML/MPLDEngine.hpp"
#include "DmlProgram/BaselineDmlProgramRuntime.hpp"
#include <graphics/graphics.h>
#include <graphics/vec2.h>
#include <winrt/base.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <d3d12.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

namespace BackgroundRemoval::Generated {
auto makeMediaPipeLandscapeDmlProgram() -> std::unique_ptr<BaselineDml::BaselineDmlProgramRuntime>;
}
namespace BackgroundRemoval::MPLDDetail {
extern const char *renderingEffectString;
extern const char *gateEffectString;
}
using Microsoft::WRL::ComPtr;
namespace LandscapeBenchmark {
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

struct Options {
	bool verifyReference = true;
	int samples = 100, warmup = 20;
	std::uint32_t width = 1280, height = 720;
	std::filesystem::path output;
};
auto parseOptions(int argc, char **argv) -> Options
{
	Options options;
	for (int i = 1; i < argc; ++i) {
		std::string name = argv[i];
		if (name == "--pix-engine") {
			// PIX captures the first D3D12 device: omit the separate reference Runtime.
			options.verifyReference = false;
			options.samples = 2;
			options.warmup = 0;
			continue;
		}
		if (++i == argc)
			throw std::invalid_argument("Missing benchmark option value");
		if (name == "--output")
			options.output = argv[i];
		else if (name == "--samples")
			options.samples = std::stoi(argv[i]);
		else if (name == "--warmup")
			options.warmup = std::stoi(argv[i]);
		else if (name == "--width")
			options.width = std::stoul(argv[i]);
		else if (name == "--height")
			options.height = std::stoul(argv[i]);
		else
			throw std::invalid_argument("Unknown benchmark option");
	}
	if (options.samples < 1 || options.samples > 10000 || options.warmup < 0 || options.warmup > 1000 ||
	    options.width < 1 || options.width > 4096 || options.height < 1 || options.height > 2160)
		throw std::invalid_argument("Invalid benchmark configuration");
	if (options.output.empty())
		options.output = "landscape-benchmark-" +
				 std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
	if (std::filesystem::exists(options.output / "samples.csv"))
		throw std::invalid_argument("Output already contains samples.csv; select a new directory");
	std::filesystem::create_directories(options.output);
	return options;
}

// Only used after the timed frame has completed. Never part of the hot path.
auto readModelOutput(BackgroundRemoval::MPLDEngine &engine) -> std::vector<float>
{
	auto *device = engine.device();
	D3D12_HEAP_PROPERTIES heap{};
	heap.Type = D3D12_HEAP_TYPE_READBACK;
	auto desc = engine.gpuOutput()->GetDesc();
	desc.Flags = D3D12_RESOURCE_FLAG_NONE;
	ComPtr<ID3D12Resource> readback;
	winrt::check_hresult(device->CreateCommittedResource(
		&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback)));
	ComPtr<ID3D12CommandAllocator> allocator;
	winrt::check_hresult(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));
	ComPtr<ID3D12GraphicsCommandList> commands;
	winrt::check_hresult(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr,
						       IID_PPV_ARGS(&commands)));
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition = {engine.gpuOutput(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
			      D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE};
	commands->ResourceBarrier(1, &barrier);
	commands->CopyResource(readback.Get(), engine.gpuOutput());
	std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
	commands->ResourceBarrier(1, &barrier);
	winrt::check_hresult(commands->Close());
	ComPtr<ID3D12Fence> completion;
	winrt::check_hresult(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&completion)));
	std::unique_ptr<void, decltype(&CloseHandle)> event{CreateEventW(nullptr, FALSE, FALSE, nullptr), CloseHandle};
	if (!event)
		winrt::throw_last_error();
	ID3D12CommandList *lists[]{commands.Get()};
	engine.commandQueue()->ExecuteCommandLists(1, lists);
	winrt::check_hresult(engine.commandQueue()->Signal(completion.Get(), 1));
	winrt::check_hresult(completion->SetEventOnCompletion(1, event.get()));
	if (WaitForSingleObject(event.get(), INFINITE) != WAIT_OBJECT_0)
		throw std::runtime_error("Verification wait failed");
	winrt::check_hresult(device->GetDeviceRemovedReason());
	D3D12_RANGE range{0, 256 * 144 * sizeof(float)};
	void *mapped = nullptr;
	winrt::check_hresult(readback->Map(0, &range, &mapped));
	std::vector<float> result(256 * 144);
	std::memcpy(result.data(), mapped, range.End);
	D3D12_RANGE noWrite{0, 0};
	readback->Unmap(0, &noWrite);
	return result;
}

struct Sample {
	double inputDraw = 0, gate = 0, submit = 0, independent = 0;
	double refine = 0, wait = 0, complete = 0, draw = 0, total = 0, drain = 0, completedFrame = 0;
	bool blocked = false;
};
constexpr std::array metrics{std::pair{"input_draw_ms", &Sample::inputDraw},
			     std::pair{"gate_ms", &Sample::gate},
			     std::pair{"submit_ms", &Sample::submit},
			     std::pair{"independent_ms", &Sample::independent},
			     std::pair{"final_wait_ms", &Sample::wait},
			     std::pair{"complete_ms", &Sample::complete},
			     std::pair{"refine_submit_ms", &Sample::refine},
			     std::pair{"draw_submit_ms", &Sample::draw},
			     std::pair{"cpu_frame_ms", &Sample::total},
			     std::pair{"draw_drain_ms", &Sample::drain},
			     std::pair{"completed_frame_ms", &Sample::completedFrame}};

auto milliseconds(std::chrono::steady_clock::time_point start) -> double
{
	return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

int run(const Options &options)
{
	winrt::init_apartment();
	struct Apartment {
		~Apartment() { winrt::uninit_apartment(); }
	} apartment;
	graphics_t *graphics = nullptr;
	if (gs_create(&graphics, OBS_TEST_BIN_DIR "/libobs-d3d11.dll", 0) != 0)
		throw std::runtime_error("OBS graphics creation failed");
	struct GraphicsCleanup {
		graphics_t *graphics;
		~GraphicsCleanup()
		{
			gs_set_render_target(nullptr, nullptr);
			gs_leave_context();
			gs_destroy(graphics);
		}
	} cleanup{graphics};
	gs_enter_context(graphics);
	std::unique_ptr<BaselineDml::BaselineDmlProgramRuntime> referenceRuntime;
	if (options.verifyReference)
		referenceRuntime = BackgroundRemoval::Generated::makeMediaPipeLandscapeDmlProgram();
	BackgroundRemoval::MPLDEngine engine(
		BackgroundRemoval::MPLDEngine::Output::GpuMask, options.width, options.height);
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> alpha{gs_texture_wrap_obj(engine.maskTexture()),
								      gs_texture_destroy};
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> sourceDraw{
		gs_texture_create(options.width, options.height, GS_BGRA, 1, nullptr, GS_RENDER_TARGET),
		gs_texture_destroy};
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> finalDraw{
		gs_texture_create(options.width, options.height, GS_BGRA, 1, nullptr, GS_RENDER_TARGET),
		gs_texture_destroy};
	std::unique_ptr<gs_stagesurf_t, void (*)(gs_stagesurf_t *)> finalReadback{
		gs_stagesurface_create(options.width, options.height, GS_BGRA), gs_stagesurface_destroy};
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> last{
		gs_texture_create(256, 144, GS_BGRA, 1, nullptr, GS_RENDER_TARGET), gs_texture_destroy};
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> current{
		gs_texture_create(256, 144, GS_BGRA, 1, nullptr, GS_RENDER_TARGET), gs_texture_destroy};
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> difference{
		gs_texture_create(256, 144, GS_R8, 1, nullptr, GS_RENDER_TARGET), gs_texture_destroy};
	std::unique_ptr<gs_stagesurf_t, void (*)(gs_stagesurf_t *)> differenceReadback{
		gs_stagesurface_create(256, 144, GS_R8), gs_stagesurface_destroy};
	std::unique_ptr<gs_effect_t, void (*)(gs_effect_t *)> effect{
		gs_effect_create(BackgroundRemoval::MPLDDetail::renderingEffectString, "LandscapeBenchmark.effect", nullptr),
		gs_effect_destroy};
	std::unique_ptr<gs_effect_t, void (*)(gs_effect_t *)> gateEffect{
		gs_effect_create(BackgroundRemoval::MPLDDetail::gateEffectString, "LandscapeBenchmarkGate.effect", nullptr),
		gs_effect_destroy};
	if (!alpha || !sourceDraw || !finalDraw || !finalReadback || !last || !current || !difference || !differenceReadback ||
	    !effect || !gateEffect)
		throw std::runtime_error("Benchmark graphics resource creation failed");
	auto *image = gs_effect_get_param_by_name(effect.get(), "image");
	auto *mask = gs_effect_get_param_by_name(effect.get(), "image1");
	auto *gateImage = gs_effect_get_param_by_name(gateEffect.get(), "image");
	auto *gateImage1 = gs_effect_get_param_by_name(gateEffect.get(), "image1");
	if (!image || !mask || !gateImage || !gateImage1)
		throw std::runtime_error("Missing benchmark effect parameter");
	const auto input = portraitInput();
	std::array<std::vector<float>, 2> expected{std::vector<float>(256 * 144), std::vector<float>(256 * 144)};
	auto createFixture = [&](int variant) -> std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> {
		std::vector<std::uint8_t> pixels(256 * 144 * 4, 255);
		std::vector<float> values(input.size());
		for (std::size_t p = 0; p < 256 * 144; ++p)
			for (std::size_t c = 0; c < 3; ++c) {
				auto value = static_cast<std::uint8_t>(std::round(
					(variant ? 1 - input[c * 256 * 144 + p] : input[c * 256 * 144 + p]) * 255));
				pixels[4 * p + 2 - c] = value;
				values[c * 256 * 144 + p] = value / 255.0F;
			}
		const auto *bytes = pixels.data();
		std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> texture{
			gs_texture_create(256, 144, GS_BGRA, 1, &bytes, 0), gs_texture_destroy};
		if (!texture)
			throw std::runtime_error("Fixture texture creation failed");
		const std::array inputs{std::as_bytes(std::span(values))};
		if (referenceRuntime)
			referenceRuntime->run(inputs, std::as_writable_bytes(std::span(expected[variant])));
		return texture;
	};
	std::array<std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)>, 2> fixtures{createFixture(0),
											createFixture(1)};
	auto draw = [&](gs_texture_t *destination, gs_texture_t *source, const char *technique, std::uint32_t width,
			std::uint32_t height) {
		gs_set_render_target_with_color_space(destination, nullptr, GS_CS_709_EXTENDED);
		gs_set_viewport(0, 0, width, height);
		gs_ortho(0, static_cast<float>(width), 0, static_cast<float>(height), -100, 100);
		gs_matrix_identity();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
		while (gs_effect_loop(effect.get(), technique)) {
			gs_effect_set_texture(image, source);
			if (std::strcmp(technique, "DrawWithoutBlur") == 0)
				gs_effect_set_texture(mask, alpha.get());
			gs_draw_sprite(nullptr, 0, width, height);
		}
	};
	auto drawDifference = [&](gs_texture_t *left, gs_texture_t *right) {
		gs_set_render_target_with_color_space(difference.get(), nullptr, GS_CS_709_EXTENDED);
		gs_set_viewport(0, 0, 256, 144);
		gs_ortho(0, 256, 0, 144, -100, 100);
		gs_matrix_identity();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
		while (gs_effect_loop(gateEffect.get(), "LumaAbsDiff")) {
			gs_effect_set_texture(gateImage, left);
			gs_effect_set_texture(gateImage1, right);
			gs_draw_sprite(nullptr, 0, 256, 144);
		}
	};
	double maxError = 0;
	auto verify = [&](int variant) {
		if (!referenceRuntime)
			return;
		const auto actual = readModelOutput(engine);
		for (std::size_t i = 0; i < actual.size(); ++i) {
			if (!std::isfinite(actual[i]))
				throw std::runtime_error("Non-finite model output");
			maxError = std::max(maxError, std::abs(double(actual[i]) - expected[variant][i]));
		}
		if (maxError > 2e-5)
			throw std::runtime_error("GPU-path model output differs from reference");
	};
	std::ofstream csv(options.output / "samples.csv"), summary(options.output / "summary.csv");
	csv.exceptions(std::ios::badbit | std::ios::failbit);
	summary.exceptions(std::ios::badbit | std::ios::failbit);
	csv << std::setprecision(10) << "scenario,iteration,width,height,inferred,refined,blocked,difference_sum";
	for (const auto &[name, field] : metrics)
		csv << ',' << name;
	csv << '\n';
	summary << std::setprecision(10) << "scenario,metric,n,mean_ms,median_ms,p95_ms\n";
	for (const std::string scenario : {"infer", "skip", "settings-only"}) {
		float threshold = .5F;
		draw(last.get(), fixtures[0].get(), "Draw", 256, 144);
		winrt::check_hresult(engine.runAsync(last.get(), true, threshold));
		winrt::check_hresult(engine.blockCpuUntilSubmittedGpuWorkCompletes());
		winrt::check_hresult(engine.complete());
		verify(0);
		drawDifference(last.get(), last.get());
		gs_stage_texture(differenceReadback.get(), difference.get());
		std::vector<Sample> samples;
		std::size_t blocked = 0, submitted = 0;
		for (int iteration = -options.warmup; iteration < options.samples; ++iteration) {
			const int step = iteration + options.warmup;
			const int variant = scenario == "infer" ? (step % 2 == 0 ? 1 : 0) : 0;
			const bool refine = scenario != "skip";
			if (scenario == "settings-only")
				threshold = step % 2 == 0 ? .45F : .55F;
			Sample sample;
			const auto frameStart = std::chrono::steady_clock::now();
			std::uint8_t *differenceData = nullptr;
			std::uint32_t differencePitch = 0;
			std::uint64_t differenceSum = 0;
			if (!gs_stagesurface_map(differenceReadback.get(), &differenceData, &differencePitch))
				throw std::runtime_error("Difference Map failed");
			const bool validDifference = differenceData && differencePitch >= 256;
			if (validDifference)
				for (std::uint32_t y = 0; y < 144; ++y)
					for (std::uint32_t x = 0; x < 256; ++x)
						differenceSum += differenceData[std::size_t(y) * differencePitch + x];
			gs_stagesurface_unmap(differenceReadback.get());
			if (!validDifference)
				throw std::runtime_error("Invalid difference readback");
			sample.gate = milliseconds(frameStart);
			auto section = std::chrono::steady_clock::now();
			draw(current.get(), fixtures[variant].get(), "Draw", 256, 144);
			sample.inputDraw = milliseconds(section);
			section = std::chrono::steady_clock::now();
			drawDifference(last.get(), current.get());
			gs_stage_texture(differenceReadback.get(), difference.get());
			sample.gate += milliseconds(section);
			section = std::chrono::steady_clock::now();
			const bool infer = scenario == "infer";
			if (infer)
				winrt::check_hresult(engine.runAsync(current.get(), true, threshold));
			else if (refine)
				winrt::check_hresult(engine.refineAsync(true, threshold));
			sample.submit = milliseconds(section);
			section = std::chrono::steady_clock::now();
			draw(sourceDraw.get(), fixtures[variant].get(), "Draw", options.width, options.height);
			sample.independent = milliseconds(section);
			if (infer || refine) {
				section = std::chrono::steady_clock::now();
				bool done = false;
				winrt::check_hresult(engine.isDone(done));
				sample.blocked = !done;
				if (sample.blocked)
					winrt::check_hresult(engine.blockCpuUntilSubmittedGpuWorkCompletes());
				sample.wait = milliseconds(section);
				section = std::chrono::steady_clock::now();
				winrt::check_hresult(engine.complete());
				sample.complete = milliseconds(section);
			}
			std::swap(last, current);
			section = std::chrono::steady_clock::now();
			draw(finalDraw.get(), sourceDraw.get(), "DrawWithoutBlur", options.width, options.height);
			gs_flush();
			sample.draw = milliseconds(section);
			sample.total = milliseconds(frameStart);
			// Drain after CPU frame timing so final draws cannot build up across samples.
			section = std::chrono::steady_clock::now();
			gs_set_render_target(nullptr, nullptr);
			gs_stage_texture(finalReadback.get(), finalDraw.get());
			std::uint8_t *data = nullptr;
			std::uint32_t pitch = 0;
			if (!gs_stagesurface_map(finalReadback.get(), &data, &pitch))
				throw std::runtime_error("Final draw drain failed");
			const bool finalValid = data && pitch >= options.width * 4;
			gs_stagesurface_unmap(finalReadback.get());
			if (!finalValid)
				throw std::runtime_error("Invalid final draw readback");
			sample.drain = milliseconds(section);
			sample.completedFrame = milliseconds(frameStart);
			if (iteration >= 0) {
				samples.push_back(sample);
				blocked += sample.blocked;
				submitted += infer || refine;
				csv << scenario << ',' << iteration << ',' << options.width << ',' << options.height
				    << ',' << infer << ',' << refine << ',' << sample.blocked << ',' << differenceSum;
				for (const auto &[name, field] : metrics)
					csv << ',' << sample.*field;
				csv << '\n';
			}
			if (infer && (iteration == 0 || iteration == options.samples - 1))
				verify(variant);
			Sleep(10);
		}
		for (const auto &[name, field] : metrics) {
			std::vector<double> values;
			for (const auto &sample : samples)
				values.push_back(sample.*field);
			std::sort(values.begin(), values.end());
			const double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
			const double median = (values[(values.size() - 1) / 2] + values[values.size() / 2]) / 2;
			const double p95 = values[(values.size() * 95 + 99) / 100 - 1];
			summary << scenario << ',' << name << ',' << values.size() << ',' << mean << ',' << median
				<< ',' << p95 << '\n';
			std::cout << scenario << ',' << name << ",n=" << values.size() << ",mean_ms=" << mean
				  << ",median_ms=" << median << ",p95_ms=" << p95 << '\n';
		}
		std::cout << scenario << ",blocked=" << blocked << '/' << submitted
			  << ",completed_before_deadline=" << submitted - blocked << '/' << submitted << '\n';
	}
	std::ofstream metadata(options.output / "conditions.txt");
	metadata.exceptions(std::ios::badbit | std::ios::failbit);
	metadata << "width=" << options.width << "\nheight=" << options.height << "\nsamples=" << options.samples
		 << "\nwarmup=" << options.warmup << "\nverification_enabled=" << options.verifyReference << "\nmodel_max_abs_error=" << maxError
		 << "\nfixture=" << LANDSCAPE_PORTRAIT << "\nsensitivity=40\nrefine_enabled=1\nblur_enabled=0\n";
	std::cout << (referenceRuntime ? "GPU output verification passed; max_abs_error=" : "PIX-only run; verification disabled; value=") << maxError << "\nOutput: " << options.output
		  << '\n';
	return 0;
}
} // namespace LandscapeBenchmark

int main(int argc, char **argv)
{
	if (TraceLoggingRegister(g_hProvider) != ERROR_SUCCESS)
		return 1;
	int result = 1;
	try {
		result = LandscapeBenchmark::run(LandscapeBenchmark::parseOptions(argc, argv));
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
	}
	TraceLoggingUnregister(g_hProvider);
	return result;
}
