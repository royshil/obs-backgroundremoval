// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>
#include "GpuAccessValidation.hpp"
#include "BackgroundRemoval/Operators.hpp"
#include <chrono>
#include <iostream>
#include "BackgroundRemoval/windows/MaskRefinerDirect3D.hpp"
#include "TraceLoggingProvider.hpp"
#include <d3d11_4.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>
using Microsoft::WRL::ComPtr;
namespace MaskRefinerTest {
void check(HRESULT hr)
{
	if (FAILED(hr))
		throw std::runtime_error("Test Direct3D call failed");
}
bool warp = true;
struct Device final {
	ComPtr<ID3D12Device> device;
	ComPtr<ID3D12CommandQueue> queue;
	ComPtr<ID3D11Device> graphics;
	ComPtr<ID3D11DeviceContext> context;
	Device()
	{
		ComPtr<IDXGIFactory4> factory;
		check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
		ComPtr<IDXGIAdapter> adapter;
		if (warp)
			check(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)));
		else
			check(factory->EnumAdapters(0, &adapter));
		check(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));
		D3D12_COMMAND_QUEUE_DESC desc{};
		desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		check(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&queue)));
		check(D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, nullptr, 0,
					D3D11_SDK_VERSION, &graphics, nullptr, &context));
	}
	auto upload(const std::vector<float> &input) -> ComPtr<ID3D12Resource>
	{
		D3D12_HEAP_PROPERTIES heap{};
		heap.Type = D3D12_HEAP_TYPE_UPLOAD;
		D3D12_RESOURCE_DESC desc{};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Width = input.size() * 4;
		desc.Height = desc.DepthOrArraySize = desc.MipLevels = desc.SampleDesc.Count = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		ComPtr<ID3D12Resource> resource;
		check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
						      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
						      IID_PPV_ARGS(&resource)));
		void *data = nullptr;
		D3D12_RANGE none{0, 0};
		check(resource->Map(0, &none, &data));
		std::memcpy(data, input.data(), input.size() * 4);
		resource->Unmap(0, nullptr);
		return resource;
	}
	// Input upload -> DEFAULT COPY_DEST is part of the same ordered GPU queue.
	auto input(const std::vector<float> &values) -> ComPtr<ID3D12Resource>
	{
		auto source = upload(values);
		auto desc = source->GetDesc();
		D3D12_HEAP_PROPERTIES heap{};
		heap.Type = D3D12_HEAP_TYPE_DEFAULT;
		ComPtr<ID3D12Resource> result;
		check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
						      D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&result)));
		ComPtr<ID3D12CommandAllocator> allocator;
		check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));
		ComPtr<ID3D12GraphicsCommandList> commands;
		check(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr,
						IID_PPV_ARGS(&commands)));
		commands->CopyBufferRegion(result.Get(), 0, source.Get(), 0, values.size() * 4);
		check(commands->Close());
		ID3D12CommandList *lists[]{commands.Get()};
		queue->ExecuteCommandLists(1, lists);
		ComPtr<ID3D12Fence> fence;
		check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
		check(queue->Signal(fence.Get(), 1));
		HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
		check(fence->SetEventOnCompletion(1, event));
		WaitForSingleObject(event, INFINITE);
		CloseHandle(event);
		return result;
	}
	auto read(ID3D11Texture2D *texture) -> std::vector<unsigned char>
	{
		D3D11_TEXTURE2D_DESC desc{};
		texture->GetDesc(&desc);
		desc.BindFlags = 0;
		desc.MiscFlags = 0;
		desc.Usage = D3D11_USAGE_STAGING;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		ComPtr<ID3D11Texture2D> staging;
		check(graphics->CreateTexture2D(&desc, nullptr, &staging));
		context->CopyResource(staging.Get(), texture);
		D3D11_MAPPED_SUBRESOURCE mapped{};
		check(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped));
		std::vector<unsigned char> result(size_t(desc.Width) * desc.Height);
		for (UINT y = 0; y < desc.Height; ++y)
			std::memcpy(result.data() + size_t(y) * desc.Width,
				    static_cast<unsigned char *>(mapped.pData) + size_t(y) * mapped.RowPitch,
				    desc.Width);
		context->Unmap(staging.Get(), 0);
		return result;
	}
};
auto reference(const std::vector<float> &p, unsigned iw, unsigned ih, unsigned ow, unsigned oh,
	       BackgroundRemoval::MaskRefinerDirect3D::Settings settings) -> std::vector<float>
{
	std::vector<float> a(p.size()), b(p.size()), c(size_t(ow) * oh), d(c.size());
	for (size_t i = 0; i < p.size(); ++i) {
		float v = std::clamp(1 - p[i], 0.f, 1.f);
		a[i] = settings.enabled ? float(v > 1 - settings.threshold) : v;
	}
	auto at = [](const std::vector<float> &v, int x, int y, unsigned w, unsigned h) {
		return v[size_t(std::clamp(y, 0, int(h) - 1)) * w + std::clamp(x, 0, int(w) - 1)];
	};
	if (settings.enabled) {
		float weights[9], total = 0;
		for (int k = -4; k <= 4; ++k) {
			weights[k + 4] = std::exp(-float(k * k) / (2 * 1.7f * 1.7f));
			total += weights[k + 4];
		}
		for (unsigned y = 0; y < ih; ++y)
			for (unsigned x = 0; x < iw; ++x) {
				float sum = 0;
				for (int k = -4; k <= 4; ++k)
					sum += weights[k + 4] * at(a, int(x) + k, y, iw, ih);
				b[size_t(y) * iw + x] = sum / total;
			}
		for (unsigned y = 0; y < ih; ++y)
			for (unsigned x = 0; x < iw; ++x) {
				float sum = 0;
				for (int k = -4; k <= 4; ++k)
					sum += weights[k + 4] * at(b, x, int(y) + k, iw, ih);
				a[size_t(y) * iw + x] = sum / total;
			}
	}
	for (unsigned y = 0; y < oh; ++y)
		for (unsigned x = 0; x < ow; ++x) {
			float sx = (float(x) + .5f) * iw / ow - .5f, sy = (float(y) + .5f) * ih / oh - .5f;
			int ix = int(std::floor(sx)), iy = int(std::floor(sy));
			float fx = sx - ix, fy = sy - iy;
			float t = at(a, ix, iy, iw, ih) * (1 - fx) + at(a, ix + 1, iy, iw, ih) * fx;
			float u = at(a, ix, iy + 1, iw, ih) * (1 - fx) + at(a, ix + 1, iy + 1, iw, ih) * fx;
			float v = t * (1 - fy) + u * fy;
			c[size_t(y) * ow + x] = settings.enabled ? float(v > .5f) : v;
		}
	if (settings.enabled && settings.expansion) {
		int radius = std::abs(settings.expansion);
		// Independent square neighborhood reference, not the GPU's separable two passes.
		for (unsigned y = 0; y < oh; ++y)
			for (unsigned x = 0; x < ow; ++x) {
				float value = settings.expansion > 0 ? 1.f : 0.f;
				for (int dy = -radius; dy <= radius; ++dy)
					for (int dx = -radius; dx <= radius; ++dx) {
						float v = at(c, int(x) + dx, int(y) + dy, ow, oh);
						value = settings.expansion > 0 ? std::min(value, v)
									       : std::max(value, v);
					}
				d[size_t(y) * ow + x] = value;
			}
		c.swap(d);
	}
	return c;
}
} // namespace MaskRefinerTest
TEST_CASE("MaskRefinerDirect3D")
{
	using namespace MaskRefinerTest;
	Device gpu;
	constexpr unsigned iw = 32, ih = 24, ow = 47, oh = 35;
	BackgroundRemoval::MaskRefinerDirect3D refiner(gpu.device.Get(), gpu.queue.Get(), gpu.graphics.Get(), iw, ih,
						       ow, oh);
	CHECK_FALSE(refiner.isDone());
	CHECK_THROWS_AS(refiner.complete(), std::logic_error);
	CHECK_THROWS_AS(refiner.blockCpuUntilComplete(), std::logic_error);
	refiner.runAsync(nullptr, {});
	CHECK_THROWS_AS(refiner.runAsync(nullptr, {}), std::logic_error);
	refiner.blockCpuUntilComplete();
	refiner.complete();
	auto initial = gpu.read(refiner.outputTexture());
	CHECK(std::all_of(initial.begin(), initial.end(), [](auto v) { return v == 255; }));
	refiner.runAsync(nullptr, {true, 0.0F, 30});
	refiner.blockCpuUntilComplete();
	refiner.complete();
	CHECK(gpu.read(refiner.outputTexture()) == initial);
	std::vector<float> values(iw * ih);
	for (unsigned y = 0; y < ih; ++y)
		for (unsigned x = 0; x < iw; ++x)
			values[y * iw + x] = (x >= 8 && x < 24 && y >= 5 && y < 20) ? .9f : .1f;
	values[0] = -.2f;
	values.back() = 1.2f;
	auto input = gpu.input(values);
	auto upload = gpu.upload(values);
	CHECK_THROWS_AS(refiner.runAsync(upload.Get(), {}), std::invalid_argument);
	for (const auto settings : {BackgroundRemoval::MaskRefinerDirect3D::Settings{false, .42f, 0},
				    {true, .42f, 0},
				    {true, .42f, 2},
				    {true, .42f, -2},
				    {true, .42f, 30},
				    {true, .42f, -30}}) {
		INFO("enabled=" << settings.enabled << ", expansion=" << settings.expansion);
		refiner.runAsync(input.Get(), settings);
		refiner.blockCpuUntilComplete();
		refiner.complete();
		auto actual = gpu.read(refiner.outputTexture());
		auto expected = reference(values, iw, ih, ow, oh, settings);
		unsigned maximum = 0;
		for (size_t i = 0; i < actual.size(); ++i)
			maximum = std::max(maximum,
					   unsigned(std::abs(int(actual[i]) - int(std::lround(expected[i] * 255)))));
		CHECK(maximum <= 1);
		// Reusing retained probabilities must produce exactly the same texture.
		refiner.runAsync(nullptr, settings);
		refiner.blockCpuUntilComplete();
		refiner.complete();
		CHECK(gpu.read(refiner.outputTexture()) == actual);
	}
	CHECK_THROWS_AS(refiner.runAsync(nullptr, {true, .5f, 31}), std::invalid_argument);
	CHECK_THROWS_AS(refiner.runAsync(nullptr, {true, std::numeric_limits<float>::quiet_NaN(), 0}),
			std::invalid_argument);
	{
		BackgroundRemoval::MaskRefinerDirect3D large(gpu.device.Get(), gpu.queue.Get(), gpu.graphics.Get(), iw,
							     ih, 1920, 1080);
		large.runAsync(input.Get(), {});
		large.blockCpuUntilComplete();
		large.complete();
		const auto actual = gpu.read(large.outputTexture());
		const auto expected = reference(values, iw, ih, 1920, 1080, {});
		unsigned maximum = 0;
		for (size_t i = 0; i < actual.size(); ++i)
			maximum = std::max(maximum,
					   unsigned(std::abs(int(actual[i]) - int(std::lround(expected[i] * 255)))));
		CHECK(maximum <= 1);
	}
	values[0] = std::numeric_limits<float>::quiet_NaN();
	auto invalid = gpu.input(values);
	refiner.runAsync(invalid.Get(), {});
	refiner.blockCpuUntilComplete();
	CHECK_THROWS_AS(refiner.complete(), std::runtime_error);
	// Recovery after rejected data and destruction with pending work.
	refiner.runAsync(input.Get(), {});
	refiner.blockCpuUntilComplete();
	refiner.complete();
	{
		BackgroundRemoval::MaskRefinerDirect3D pending(gpu.device.Get(), gpu.queue.Get(), gpu.graphics.Get(),
							       iw, ih, ow, oh);
		pending.runAsync(input.Get(), {});
	}
	GpuAccessValidation::checkMessages(gpu.device.Get());
}
TEST_CASE("MaskRefinerBenchmark" * doctest::skip())
{
	using namespace MaskRefinerTest;
	Device gpu;
	using Clock = std::chrono::steady_clock;
	constexpr unsigned iw = 256, ih = 144;
	std::vector<float> probabilities(iw * ih);
	for (unsigned y = 0; y < ih; ++y)
		for (unsigned x = 0; x < iw; ++x)
			probabilities[y * iw + x] =
				(x > iw / 4 && x < 3 * iw / 4 && y > ih / 6 && y < 5 * ih / 6) ? .9f : .1f;
	auto input = gpu.input(probabilities);
	for (unsigned ow : {1280u, 1920u})
		for (int expansion : {0, 2}) {
			unsigned oh = ow * 9 / 16;
			BackgroundRemoval::MaskRefinerDirect3D refiner(gpu.device.Get(), gpu.queue.Get(),
								       gpu.graphics.Get(), iw, ih, ow, oh);
			BackgroundRemoval::MaskRefinerDirect3D::Settings settings{true, .5f, expansion};
			std::vector<std::uint8_t> mask(iw * ih), smoothed(mask.size()), resized(size_t(ow) * oh);
			Operators::ThresholdU8 modelThreshold({iw, ih}), outputThreshold({ow, oh});
			Operators::SmoothU8 smooth({iw, ih});
			Operators::ResizeU8 resize;
			Operators::MorphologyU8 morphology({ow, oh});
			D3D11_TEXTURE2D_DESC desc{};
			desc.Width = ow;
			desc.Height = oh;
			desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
			desc.Format = DXGI_FORMAT_R8_UNORM;
			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			ComPtr<ID3D11Texture2D> uploaded;
			check(gpu.graphics->CreateTexture2D(&desc, nullptr, &uploaded));
			auto cpu = [&] {
				for (size_t i = 0; i < mask.size(); ++i)
					mask[i] = std::uint8_t(std::clamp((1 - probabilities[i]) * 255.f, 0.f, 255.f));
				REQUIRE(modelThreshold.apply(mask.data(), iw, mask.data(), iw, 127));
				REQUIRE(smooth.apply(mask.data(), iw, smoothed.data(), iw));
				REQUIRE(resize.apply(smoothed.data(), {iw, ih, iw}, resized.data(), {ow, oh, ow}));
				REQUIRE(outputThreshold.apply(resized.data(), ow, resized.data(), ow, 128));
				if (expansion) {
					REQUIRE(morphology.begin(resized.data(), ow));
					for (int i = 0; i < expansion; ++i)
						REQUIRE(morphology.erode());
					REQUIRE(morphology.end(resized.data(), ow));
				}
				D3D11_MAPPED_SUBRESOURCE mapped{};
				check(gpu.context->Map(uploaded.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
				for (unsigned y = 0; y < oh; ++y)
					std::memcpy(static_cast<std::uint8_t *>(mapped.pData) +
							    size_t(y) * mapped.RowPitch,
						    resized.data() + size_t(y) * ow, ow);
				gpu.context->Unmap(uploaded.Get(), 0);
			};
			auto runGpu = [&] {
				refiner.runAsync(input.Get(), settings);
				refiner.blockCpuUntilComplete();
				refiner.complete();
			};
			for (int i = 0; i < 5; ++i) {
				cpu();
				runGpu();
			}
			std::vector<double> timings[2];
			for (int i = 0; i < 40; ++i)
				for (int turn = 0; turn < 2; ++turn) {
					int method = (i + turn) % 2;
					auto start = Clock::now();
					if (method)
						runGpu();
					else
						cpu();
					timings[method].push_back(
						std::chrono::duration<double, std::milli>(Clock::now() - start).count());
				}
			for (int method = 0; method < 2; ++method) {
				auto v = timings[method];
				std::sort(v.begin(), v.end());
				double sum = 0;
				for (auto t : v)
					sum += t;
				std::cout << "MASK_BENCH,width=" << ow << ",height=" << oh << ",expansion=" << expansion
					  << ",method=" << (method ? "gpu" : "cpu-upload") << ",n=" << v.size()
					  << ",mean_ms=" << sum / v.size() << ",median_ms=" << (v[19] + v[20]) / 2
					  << ",p95_ms=" << v[37] << "\n";
			}
			auto actual = gpu.read(refiner.outputTexture());
			size_t mismatches = 0;
			for (size_t i = 0; i < actual.size(); ++i)
				mismatches += (actual[i] >= 128) != (resized[i] >= 128);
			std::cout << "MASK_DIFFERENCE,width=" << ow << ",expansion=" << expansion
				  << ",pixels=" << actual.size() << ",different=" << mismatches << "\n";
		}
}

int main(int argc, char **argv)
{
	GpuAccessValidation::enable();
	TraceLoggingRegister(provider);
	doctest::Context context;
	for (int i = 1; i < argc; ++i)
		if (std::strcmp(argv[i], "--hardware") == 0)
			MaskRefinerTest::warp = false;
	for (int i = 1; i < argc; ++i)
		if (std::strcmp(argv[i], "--benchmark") == 0) {
			context.setOption("test-case", "MaskRefinerBenchmark");
			context.setOption("no-skip", true);
		}
	const int result = context.run();
	TraceLoggingUnregister(provider);
	return result;
}
