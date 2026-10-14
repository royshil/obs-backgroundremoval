// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
// SPDX-License-Identifier: GPL-3.0-or-later
#include "D3D12TextureReader.hpp"
#include "DmlProgram/PerformanceTrace.hpp"
#include <d3d11_4.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <stdexcept>
#include <string>

namespace BackgroundRemoval::MediaPipeLandscapeDirectML {
namespace {
using Microsoft::WRL::ComPtr;
void check(HRESULT result, const char *operation)
{
	if (result != S_OK)
		throw std::runtime_error(std::string(operation) + " failed (HRESULT " + std::to_string(result) + ")");
}
struct CloseHandleDeleter {
	void operator()(void *value) const noexcept
	{
		if (value)
			CloseHandle(value);
	}
};
using Handle = std::unique_ptr<void, CloseHandleDeleter>;
auto sharedHandle(ID3D12Device *device, ID3D12DeviceChild *object) -> Handle
{
	HANDLE handle = nullptr;
	check(device->CreateSharedHandle(object, nullptr, GENERIC_ALL, nullptr, &handle), "Create NT shared handle");
	return Handle(handle);
}
} // namespace

struct D3D12TextureReader::Impl {
	std::uint32_t width, height;
	ComPtr<ID3D12Device> device;
	ComPtr<ID3D12CommandQueue> queue;
	ComPtr<ID3D12Resource> texture;
	ComPtr<ID3D12Fence> ready, consumed;
	ComPtr<ID3D11Device5> producer;
	ComPtr<ID3D11DeviceContext4> context;
	ComPtr<ID3D11Texture2D> destination;
	ComPtr<ID3D11Fence> ready11, consumed11;
	std::uint64_t readyValue = 0, consumedValue = 0;
	Handle event{CreateEventW(nullptr, FALSE, FALSE, nullptr)};

	Impl(ID3D12Device *d, ID3D12CommandQueue *q, std::uint32_t w, std::uint32_t h)
		: width(w),
		  height(h),
		  device(d),
		  queue(q)
	{
		if (!d || !q || !w || !h || !event || gs_get_device_type() != GS_DEVICE_DIRECT3D_11)
			throw std::invalid_argument("Invalid D3D12 texture reader or OBS backend");
		ComPtr<ID3D12Device> queueDevice;
		check(q->GetDevice(IID_PPV_ARGS(&queueDevice)), "Get reader queue device");
		if (queueDevice.Get() != d || q->GetDesc().Type != D3D12_COMMAND_LIST_TYPE_DIRECT)
			throw std::invalid_argument("Texture reader requires the runtime's direct queue");
		// OBS 29 exports native D3D11 objects in graphics.h. Use its immediate
		// context so the copy follows OBS rendering on the same command stream.
		auto *native = static_cast<ID3D11Device *>(gs_get_device_obj());
		if (!native)
			throw std::runtime_error("OBS D3D11 device is unavailable");
		check(native->QueryInterface(IID_PPV_ARGS(&producer)), "OBS D3D11 shared-fence support");
		ComPtr<IDXGIDevice> dxgiDevice;
		check(producer.As(&dxgiDevice), "Get OBS DXGI device");
		ComPtr<IDXGIAdapter> adapter;
		check(dxgiDevice->GetAdapter(&adapter), "Get OBS GPU adapter");
		DXGI_ADAPTER_DESC adapterDesc{};
		check(adapter->GetDesc(&adapterDesc), "Get OBS GPU adapter description");
		const auto runtimeLuid = d->GetAdapterLuid();
		if (adapterDesc.AdapterLuid.LowPart != runtimeLuid.LowPart ||
		    adapterDesc.AdapterLuid.HighPart != runtimeLuid.HighPart)
			throw std::invalid_argument("OBS and DirectML must use the same GPU adapter");
		ComPtr<ID3D11DeviceContext> nativeContext;
		producer->GetImmediateContext(&nativeContext);
		check(nativeContext.As(&context), "OBS D3D11 context shared-fence support");

		D3D12_HEAP_PROPERTIES heap{};
		heap.Type = D3D12_HEAP_TYPE_DEFAULT;
		D3D12_RESOURCE_DESC desc{};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Width = w;
		desc.Height = h;
		desc.DepthOrArraySize = desc.MipLevels = desc.SampleDesc.Count = 1;
		desc.Format = DXGI_FORMAT_R32_FLOAT;
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;
		check(d->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_SHARED, &desc, D3D12_RESOURCE_STATE_COMMON,
						 nullptr, IID_PPV_ARGS(&texture)),
		      "Create D3D12 shared R32F texture");
		auto textureHandle = sharedHandle(d, texture.Get());
		check(producer->OpenSharedResource1(textureHandle.get(), IID_PPV_ARGS(&destination)),
		      "Open D3D12 texture in D3D11");
		check(d->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&ready)), "Create input-ready fence");
		check(d->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&consumed)),
		      "Create input-consumed fence");
		auto readyHandle = sharedHandle(d, ready.Get());
		auto consumedHandle = sharedHandle(d, consumed.Get());
		check(producer->OpenSharedFence(readyHandle.get(), IID_PPV_ARGS(&ready11)), "Open input-ready fence");
		check(producer->OpenSharedFence(consumedHandle.get(), IID_PPV_ARGS(&consumed11)),
		      "Open input-consumed fence");
	}

	~Impl()
	{
		// Also covers a captured frame for which command recording threw.
		if (ready && ready->GetCompletedValue() < readyValue &&
		    SUCCEEDED(ready->SetEventOnCompletion(readyValue, event.get()))) {
			while (WaitForSingleObject(event.get(), 1000) == WAIT_TIMEOUT) {
				if (FAILED(device->GetDeviceRemovedReason()))
					break;
			}
		}
	}
};

D3D12TextureReader::D3D12TextureReader(ID3D12Device *device, ID3D12CommandQueue *queue, std::uint32_t width,
				       std::uint32_t height)
	: impl_(std::make_unique<Impl>(device, queue, width, height))
{
}
D3D12TextureReader::~D3D12TextureReader() = default;

auto D3D12TextureReader::capture(gs_texture_t *source) -> ID3D12Resource *
{
	BaselineDml::PerformanceTrace::Scope scope("Pipeline.D3D12SharedCopy");
	auto &s = *impl_;
	if (!source || gs_texture_get_width(source) != s.width || gs_texture_get_height(source) != s.height ||
	    gs_texture_get_color_format(source) != GS_R32F)
		throw std::invalid_argument("D3D12 texture reader requires a matching R32F input");
	auto *nativeSource = static_cast<ID3D11Texture2D *>(gs_texture_get_obj(source));
	if (!nativeSource)
		throw std::invalid_argument("OBS input has no native D3D11 texture");
	ComPtr<ID3D11Device> sourceDevice;
	nativeSource->GetDevice(&sourceDevice);
	ComPtr<ID3D11Device> obsDevice;
	check(s.producer.As(&obsDevice), "Get OBS source device");
	if (sourceDevice.Get() != obsDevice.Get())
		throw std::invalid_argument("Input texture does not belong to the OBS device");
	D3D11_TEXTURE2D_DESC sourceDesc{};
	nativeSource->GetDesc(&sourceDesc);
	if (sourceDesc.MipLevels != 1 || sourceDesc.ArraySize != 1 || sourceDesc.SampleDesc.Count != 1 ||
	    sourceDesc.Format != DXGI_FORMAT_R32_FLOAT)
		throw std::invalid_argument("OBS input requires a single-sample R32F texture");
	// Protect the shared destination from overwrite while D3D12 consumes it.
	check(s.context->Wait(s.consumed11.Get(), s.consumedValue), "Wait for previous D3D12 input consumer");
	s.context->CopyResource(s.destination.Get(), nativeSource);
	const auto next = s.readyValue + 1;
	check(s.context->Signal(s.ready11.Get(), next), "Signal D3D11 input readiness");
	s.readyValue = next;
	s.context->Flush();
	check(s.queue->Wait(s.ready.Get(), next), "D3D12 wait for shared input");
	return s.texture.Get();
}

void D3D12TextureReader::signalConsumed()
{
	const auto next = impl_->consumedValue + 1;
	check(impl_->queue->Signal(impl_->consumed.Get(), next), "Signal D3D12 input consumed");
	impl_->consumedValue = next;
}
} // namespace BackgroundRemoval::MediaPipeLandscapeDirectML
