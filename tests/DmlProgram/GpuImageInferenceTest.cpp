// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
// SPDX-License-Identifier: GPL-3.0-or-later
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>
#include "BackgroundRemoval/windows/MediaPipeLandscapeDirectMLPipeline/GpuImageInference.hpp"
#include "ObsBridgeUtils/TextureReader.hpp"
#include "BackgroundRemoval/Operators.hpp"
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <wrl/client.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <vector>

#ifdef GPU_LANDSCAPE_MODEL
#include <wincodec.h>
#include <winrt/base.h>
namespace BaselineDml {
auto makeMediaPipeLandscapeDmlProgram() -> std::unique_ptr<BaselineDmlProgramRuntime>;
}
#endif

namespace {
using Microsoft::WRL::ComPtr;
namespace PD = BaselineDml::ProgramDefinition;
bool profileGpu = false;
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
void cpuRun(BaselineDml::BaselineDmlProgramRuntime &runtime, const std::vector<float> &input,
	    std::vector<float> &output)
{
	const std::array spans{std::as_bytes(std::span(input))};
	runtime.run(spans, std::as_writable_bytes(std::span(output)));
}
auto milliseconds(std::chrono::steady_clock::time_point start) -> double
{
	return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}
void stats(const char *scenario, const char *method, std::vector<double> values)
{
	const auto mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
	std::sort(values.begin(), values.end());
	std::cout << scenario << ' ' << method << " mean=" << mean << " median=" << values[values.size() / 2]
		  << " p95=" << values[(values.size() * 95 + 99) / 100 - 1] << " ms\n";
}
#endif
} // namespace

TEST_CASE("GpuImageGate")
{
	Graphics graphics;
	for (const auto [width, height] : {std::pair{17U, 19U}, std::pair{256U, 432U}}) {
		CAPTURE(width);
		CAPTURE(height);
		const std::array<std::uint32_t, 4> shape{1, 1, height, width};
		const PD::InputSpecItem spec{PD::DataType::Float32, PD::ItemType::RuntimeInput, 4, shape, 0};
		const PD::Node nodes[]{PD::InputItem{0}, PD::Onnx14Node{PD::Relu{PD::DataType::Float32, 0, 4, shape}}};
		BaselineDml::BaselineDmlProgramRuntime runtime({1, &spec, 2, nodes, 1}, {});
		BackgroundRemoval::MediaPipeLandscapeDirectML::D3D12TextureReader reader(
			runtime.device(), runtime.queue(), width, height);
		BackgroundRemoval::MediaPipeLandscapeDirectML::GpuImageInference inference(runtime, width, height);
		std::vector<float> baseline(width * height, 0.25F), output(baseline.size(), -1);
		auto original = texture(baseline, width, height);
		CHECK_THROWS_AS(inference.commitReference(), std::logic_error);
		REQUIRE(inference.run(reader, original.get(), 40, false, output)); // First frame forces inference.
		REQUIRE(output == baseline);
		inference.commitReference();
		std::fill(output.begin(), output.end(), -1.0F);
		CHECK_FALSE(inference.run(reader, original.get(), 40, false, output));
		CHECK(std::all_of(output.begin(), output.end(), [](float x) { return x == -1; }));
		CHECK_THROWS_AS(inference.commitReference(), std::logic_error);
		for (const float delta : {0.002F, 0.009F, 0.02F}) {
			CAPTURE(delta);
			std::vector<float> changed(baseline.size(), 0.25F + delta);
			auto input = texture(changed, width, height);
			const bool inferred = inference.run(reader, input.get(), 40, false, output);
			CHECK(inferred == (delta > 0.01F));
			if (inferred) {
				CHECK(output == changed);
				inference.commitReference();
				CHECK_FALSE(inference.run(reader, input.get(), 40, false, output));
			}
		}
		// Reusing a list must still evaluate new pixels, and a threshold change
		// must update the recorded root constants.
		auto nearbyPixels = baseline;
		for (auto &value : nearbyPixels)
			value += 0.025F;
		auto nearTexture = texture(nearbyPixels, width, height);
		CHECK_FALSE(inference.run(reader, nearTexture.get(), 30, false, output));
		CHECK_FALSE(inference.run(reader, nearTexture.get(), 30, false, output));
		REQUIRE(inference.run(reader, nearTexture.get(), 50, false, output));
		CHECK(output == nearbyPixels);
		// A successful GPU dispatch alone does not commit a failed CPU refinement.
		REQUIRE(inference.run(reader, original.get(), 40, true, output));
		REQUIRE(inference.run(reader, original.get(), 40, false, output));
		inference.commitReference();
		// Changes in each RGB plane participate in the gate.
		for (std::size_t channel = 0; channel < 3; ++channel) {
			auto changed = baseline;
			for (std::size_t i = channel * changed.size() / 3; i < (channel + 1) * changed.size() / 3; ++i)
				changed[i] += 0.1F;
			auto input = texture(changed, width, height);
			REQUIRE(inference.run(reader, input.get(), 40, false, output));
			CHECK(output == changed);
		}
		auto invalid = baseline;
		invalid.back() = std::numeric_limits<float>::quiet_NaN();
		auto nanInput = texture(invalid, width, height);
		CHECK_THROWS_AS(inference.run(reader, nanInput.get(), 40, true, output), std::runtime_error);
		REQUIRE(inference.run(reader, original.get(), 40, true, output));
		CHECK(output == baseline);
		inference.commitReference();
		CHECK_THROWS_AS(inference.run(reader, original.get(), 40, false, std::span<float>(output.data(), 1)),
				std::invalid_argument);
		checkDebugErrors(runtime.device());
	}
}

TEST_CASE("PredicatedDirectML")
{
	const std::array<std::uint32_t, 4> shape{1, 1, 1, 4};
	const PD::InputSpecItem spec{PD::DataType::Float32, PD::ItemType::RuntimeInput, 4, shape, 0};
	const PD::Node nodes[]{PD::InputItem{0}, PD::Onnx14Node{PD::Relu{PD::DataType::Float32, 0, 4, shape}}};
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
TEST_CASE("GpuLandscape")
{
	Graphics graphics;
	auto runtime = BaselineDml::makeMediaPipeLandscapeDmlProgram();
	BackgroundRemoval::MediaPipeLandscapeDirectML::D3D12TextureReader reader(runtime->device(), runtime->queue(),
										 256, 432);
	BackgroundRemoval::MediaPipeLandscapeDirectML::GpuImageInference inference(*runtime, 256, 432);
	auto input = portraitInput();
	std::vector<float> expected(256 * 144), output(expected.size());
	cpuRun(*runtime, input, expected);
	auto image = texture(input, 256, 432);
	REQUIRE(inference.run(reader, image.get(), 40, false, output));
	double maxError = 0;
	REQUIRE(std::all_of(output.begin(), output.end(), [](float value) { return std::isfinite(value); }));
	for (std::size_t i = 0; i < output.size(); ++i) {
		maxError = std::max(maxError, std::abs(static_cast<double>(output[i]) - expected[i]));
	}
	std::cout << "Portrait GPU-input/CPU-input DirectML max error=" << maxError << '\n';
	CHECK(maxError < 2e-5);
	inference.commitReference();
	const auto previous = output;
	for (int i = 0; i < 5; ++i)
		CHECK_FALSE(inference.run(reader, image.get(), 40, false, output));
	CHECK(output == previous);
	for (auto &x : input)
		x = 1 - x;
	image = texture(input, 256, 432);
	REQUIRE(inference.run(reader, image.get(), 40, false, output));
	cpuRun(*runtime, input, expected);
	REQUIRE(std::all_of(output.begin(), output.end(), [](float value) { return std::isfinite(value); }));
	maxError = 0;
	for (std::size_t i = 0; i < output.size(); ++i)
		maxError = std::max(maxError, std::abs(static_cast<double>(output[i]) - expected[i]));
	CHECK(maxError < 2e-5);
	inference.commitReference();
	CHECK_FALSE(inference.run(reader, image.get(), 40, false, output));
	checkDebugErrors(runtime->device());
}

TEST_CASE("GpuLandscapePerformance" * doctest::skip())
{
	Graphics graphics;
	auto runtime = BaselineDml::makeMediaPipeLandscapeDmlProgram();
	BackgroundRemoval::MediaPipeLandscapeDirectML::D3D12TextureReader reader(runtime->device(), runtime->queue(),
										 256, 432);
	BackgroundRemoval::MediaPipeLandscapeDirectML::GpuImageInference inference(*runtime, 256, 432);
	if (profileGpu)
		inference.enableProfiling();
	ObsBridgeUtils::TextureReader cpuReader(256, 432, GS_R32F);
	Operators::PsnrF32 psnr;
	const auto baseline = portraitInput();
	auto changed = baseline;
	for (auto &x : changed)
		x = 1 - x;
	auto original = texture(baseline, 256, 432), different = texture(changed, 256, 432);
	std::vector<float> output(256 * 144);
	REQUIRE(inference.run(reader, original.get(), 40, true, output));
	inference.commitReference();
	std::ofstream csv("gpu-landscape-benchmark.csv");
	REQUIRE(csv.good());
	csv << "scenario,iteration,method,total_ms,inferred,capture_cpu_ms,record_cpu_ms,submit_cpu_ms,wait_cpu_ms,readback_cpu_ms,pack_gpu_ms,gate_gpu_ms,model_gpu_ms,packet_gpu_ms\n"
	    << std::setprecision(10);
	for (const bool change : {false, true}) {
		const char *scenario = change ? "inference-required" : "inference-skipped";
		std::vector<double> cpuTimes, gpuTimes;
		for (int iteration = -20; iteration < 100; ++iteration) {
			for (int order = 0; order < 2; ++order) {
				const bool gpu = (iteration + order) % 2 == 0;
				auto *image = change ? different.get() : original.get();
				const auto start = std::chrono::steady_clock::now();
				bool inferred;
				if (gpu)
					inferred = inference.run(reader, image, 40, false, output);
				else {
					REQUIRE(cpuReader.read(image));
					const auto &bytes = cpuReader.getBuffer();
					const auto *pixels = reinterpret_cast<const float *>(bytes.data());
					inferred = psnr.calculate(pixels, {256, 432, cpuReader.getBufferLinesize()},
								  baseline.data(), {256, 432, 1024}) <= 40;
					if (inferred) {
						const std::array spans{std::span<const std::byte>(
							reinterpret_cast<const std::byte *>(pixels), bytes.size())};
						runtime->run(spans, std::as_writable_bytes(std::span(output)));
					}
				}
				const auto duration = milliseconds(start);
				REQUIRE(inferred == change);
				if (iteration >= 0) {
					(gpu ? gpuTimes : cpuTimes).push_back(duration);
					csv << scenario << ',' << iteration << ',' << (gpu ? "gpu" : "cpu") << ','
					    << duration << ',' << inferred;
					const auto timing =
						gpu ? inference.lastTimings()
						    : BackgroundRemoval::MediaPipeLandscapeDirectML::GpuImageTimings{};
					csv << ',' << timing.captureCpuMs << ',' << timing.recordCpuMs << ','
					    << timing.submitCpuMs << ',' << timing.waitCpuMs << ','
					    << timing.readbackCpuMs << ',' << timing.packGpuMs << ','
					    << timing.gateGpuMs << ',' << timing.modelGpuMs << ',' << timing.packetGpuMs
					    << '\n';
				}
			}
			Sleep(10);
		}
		stats(scenario, "cpu", cpuTimes);
		stats(scenario, "gpu", gpuTimes);
	}
}
#endif

int main(int argc, char **argv)
{
	// Enable the standard validation layer when installed. Timing is measured
	// separately with --benchmark-no-debug to avoid instrumentation overhead.
	bool debug = true;
	std::vector<char *> arguments{argv[0]};
	for (int i = 1; i < argc; ++i) {
		if (std::string(argv[i]) == "--profile-gpu")
			profileGpu = true;
		else if (std::string(argv[i]) == "--benchmark-no-debug")
			debug = false;
		else
			arguments.push_back(argv[i]);
	}
	ComPtr<ID3D12Debug> validation;
	if (debug && SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&validation))))
		validation->EnableDebugLayer();
#ifdef GPU_LANDSCAPE_MODEL
	winrt::init_apartment();
#endif
	doctest::Context context(static_cast<int>(arguments.size()), arguments.data());
	const int result = context.run();
#ifdef GPU_LANDSCAPE_MODEL
	winrt::uninit_apartment();
#endif
	return result;
}
