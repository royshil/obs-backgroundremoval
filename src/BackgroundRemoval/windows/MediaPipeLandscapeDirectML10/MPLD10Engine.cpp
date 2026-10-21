// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "MPLD10Engine.hpp"
#include "MPLD10Model.hpp"

#include <array>
#include <cassert>
#include <cstring>
#include <cmath>
#include <stdexcept>
#include <string>

#include <d3d11_4.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#include <DmlProgram/BaselineDmlProgramRuntime.hpp>

#include <TraceLoggingActivity.h>
#include <TraceLoggingProvider.h>

TRACELOGGING_DECLARE_PROVIDER(g_hProvider);

using Microsoft::WRL::ComPtr;

namespace {

void check(HRESULT hr, const char *message)
{
	if (FAILED(hr)) {
		throw std::runtime_error(message);
	}
}

auto blockCpuUntilFenceValueReached(ID3D12Fence *fence, UINT64 value, HANDLE event) noexcept -> HRESULT
{
	assert(fence != nullptr);
	assert(event != nullptr);

	TraceLoggingThreadActivity<g_hProvider, 0x1> waiting;
	TraceLoggingWriteStart(waiting, "DirectX.CpuFenceWait");

	UINT64 completedValue = fence->GetCompletedValue();
	if (completedValue == UINT64_MAX) {
		return DXGI_ERROR_DEVICE_REMOVED;
	} else if (completedValue >= value) {
		return S_OK;
	}

	const HRESULT hr = fence->SetEventOnCompletion(value, event);
	if (FAILED(hr)) {
		return hr;
	}

	for (;;) {
		const DWORD result = WaitForSingleObject(event, 1000);
		if (result == WAIT_FAILED) {
			return HRESULT_FROM_WIN32(GetLastError());
		} else if (result != WAIT_OBJECT_0 && result != WAIT_TIMEOUT) {
			return E_UNEXPECTED;
		}

		completedValue = fence->GetCompletedValue();
		if (completedValue == UINT64_MAX) {
			return DXGI_ERROR_DEVICE_REMOVED;
		} else if (completedValue >= value) {
			return S_OK;
		}
	}
}

} // namespace

namespace BackgroundRemoval::MPLD10Detail {

void closeHandle(void *handle) noexcept
{
	if (handle)
		CloseHandle(handle);
}
auto sharedHandle(ID3D12Device *device, ID3D12DeviceChild *object) -> std::unique_ptr<void, void (*)(void *)>
{
	HANDLE handle = nullptr;
	check(device->CreateSharedHandle(object, nullptr, GENERIC_ALL, nullptr, &handle), "Create shared handle");
	return {handle, closeHandle};
}
auto buffer(ID3D12Device *device, uint64_t bytes, D3D12_RESOURCE_STATES state, D3D12_HEAP_TYPE type)
	-> ComPtr<ID3D12Resource>
{
	D3D12_HEAP_PROPERTIES heap{};
	heap.Type = type;
	D3D12_RESOURCE_DESC desc{};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Width = bytes;
	desc.Height = desc.DepthOrArraySize = desc.MipLevels = desc.SampleDesc.Count = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	if (type == D3D12_HEAP_TYPE_DEFAULT)
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	ComPtr<ID3D12Resource> value;
	check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&value)),
	      "Create engine buffer");
	return value;
}
void transition(ID3D12GraphicsCommandList *commands, ID3D12Resource *resource, D3D12_RESOURCE_STATES before,
		D3D12_RESOURCE_STATES after) noexcept
{
	if (before == after)
		return;
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition = {resource, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, before, after};
	commands->ResourceBarrier(1, &barrier);
}
constexpr char packSource[] = R"hlsl(

cbuffer Settings : register(b0) { uint width; uint count; }
Texture2D<float4> image : register(t0);
RWStructuredBuffer<float> output : register(u0);
[numthreads(256,1,1)]
void PackRgb(uint3 id : SV_DispatchThreadID) {
 if(id.x<count) {
  uint planeSize=count/3;
  uint pixel=id.x%planeSize;
  float4 rgba=image.Load(int3(pixel%width,pixel/width,0));
  output[id.x]=rgba[id.x/planeSize];
 }
}
)hlsl";

} // namespace BackgroundRemoval::MPLD10Detail

namespace BackgroundRemoval {
using namespace MPLD10Detail;

class MPLD10Engine::Impl {
public:
	static constexpr uint32_t width_ = 256, height_ = 144, count_ = width_ * height_ * 3;
	static_assert(width_ % 4 == 0);
	DXGI_FORMAT inputFormat_ = DXGI_FORMAT_UNKNOWN;
	std::unique_ptr<BaselineDml::BaselineDmlProgramRuntime> program_;
	ComPtr<ID3D12Device> device_{program_->device()};
	ComPtr<ID3D12CommandQueue> queue_{program_->queue()};
	ComPtr<ID3D12Resource> texture_, current_, readback_;
	ComPtr<ID3D12Fence> ready_, completion_;
	ComPtr<ID3D11Device5> producer_;
	ComPtr<ID3D11DeviceContext4> context_;
	ComPtr<ID3D11Texture2D> destination_;
	ComPtr<ID3D11Fence> ready11_;
	uint64_t readyValue_ = 0, fenceValue_ = 0;
	bool gpuOutput_ = false;
	bool hasSubmission_ = false;
	bool unusable_ = false;
	bool needsRecreation_ = false;
	ComPtr<ID3D12Resource> mask_;
	ComPtr<ID3D11Texture2D> mask11_;
	ComPtr<ID3D11Fence> completion11_;
	ComPtr<ID3D12Resource> settings_;
	void *settingsData_ = nullptr;
	std::unique_ptr<void, void (*)(void *)> event_{CreateEventW(nullptr, FALSE, FALSE, nullptr), closeHandle};
	ComPtr<ID3D12CommandAllocator> allocator_;
	ComPtr<ID3D12GraphicsCommandList> commands_;
	ComPtr<ID3D12DescriptorHeap> descriptors_;
	ComPtr<ID3D12RootSignature> root_;
	ComPtr<ID3D12PipelineState> pack_;
#ifdef MPLD10_GPU_DIAGNOSTICS
	ComPtr<ID3D12QueryHeap> timingQueries_;
	ComPtr<ID3D12Resource> timingReadback_;
	UINT64 timingFrequency_ = 0, timingCalibrationGpu_ = 0, timingCalibrationCpu_ = 0, timingReportedValue_ = 0;
	GUID timingFrameActivity_{};
	LARGE_INTEGER timingCpuFrequency_{}, timingSubmitted_{};
	void collectGpuTimings() noexcept;
#endif
	Impl(ID3D11Device *native, bool gpuOutput);
	void recordMaskCopy() noexcept;
	void setMaskSettings(float refineThreshold) noexcept;
	auto createSharedInputTexture(const D3D11_TEXTURE2D_DESC &input, ComPtr<ID3D12Resource> &texture) noexcept
		-> HRESULT;
	auto openSharedInputTexture(ID3D12Resource *texture, ComPtr<ID3D11Texture2D> &destination) noexcept -> HRESULT;

	auto isDone(bool &done) const noexcept -> HRESULT;
	auto getResult(std::span<float> output) noexcept -> HRESULT;
	~Impl() noexcept;
	auto record() noexcept -> HRESULT;
	auto runAsync(ID3D11Texture2D *input, const D3D11_TEXTURE2D_DESC &inputDesc, float refineThreshold) noexcept
		-> HRESULT;
	auto ensureOutput() noexcept -> HRESULT;
};

MPLD10Engine::Impl::Impl(ID3D11Device *native, bool gpuOutput)
	: program_(std::make_unique<BaselineDml::BaselineDmlProgramRuntime>(
		  MPLD10Detail::inputSpecs, MPLD10Detail::graphNodes, MPLD10Detail::uavBarriers, gpuOutput ? 239 : 236,
		  std::as_bytes(std::span(MPLD10Detail::weightsData, MPLD10Detail::weightsDataSize)))),
	  gpuOutput_(gpuOutput)
{
	auto *d = device_.Get();
	auto *q = queue_.Get();
	constexpr auto w = width_, h = height_;
	if (!d || !q || !w || !h || !event_ || !native)
		throw std::invalid_argument("Invalid DirectML engine or OBS backend");
	ComPtr<ID3D12Device> queueDevice;
	check(q->GetDevice(IID_PPV_ARGS(&queueDevice)), "Get engine queue device");
	if (queueDevice.Get() != d || q->GetDesc().Type != D3D12_COMMAND_LIST_TYPE_DIRECT)
		throw std::invalid_argument("Engine requires the Runtime's direct queue");
	// Use the caller's immediate context so input copies follow its rendering
	// on the same command stream.
	if (!native)
		throw std::runtime_error("Producer D3D11 device is unavailable");
	check(native->QueryInterface(IID_PPV_ARGS(&producer_)), "Producer D3D11 shared-fence support");
	ComPtr<IDXGIDevice> dxgiDevice;
	check(producer_.As(&dxgiDevice), "Get producer DXGI device");
	ComPtr<IDXGIAdapter> adapter;
	check(dxgiDevice->GetAdapter(&adapter), "Get producer GPU adapter");
	DXGI_ADAPTER_DESC adapterDesc{};
	check(adapter->GetDesc(&adapterDesc), "Get producer GPU adapter description");
	const auto runtimeLuid = d->GetAdapterLuid();
	if (adapterDesc.AdapterLuid.LowPart != runtimeLuid.LowPart ||
	    adapterDesc.AdapterLuid.HighPart != runtimeLuid.HighPart)
		throw std::invalid_argument("OBS and DirectML must use the same GPU adapter");
	ComPtr<ID3D11DeviceContext> nativeContext;
	producer_->GetImmediateContext(&nativeContext);
	check(nativeContext.As(&context_), "Producer D3D11 context shared-fence support");

	check(d->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&ready_)), "Create input-ready fence");
	auto readyHandle = sharedHandle(d, ready_.Get());
	check(producer_->OpenSharedFence(readyHandle.get(), IID_PPV_ARGS(&ready11_)), "Open input-ready fence");
	const auto inputType = program_->inputInfo(0).type;
	const auto outputType = program_->outputInfo().type;
	if (program_->inputCount() != 2u || inputType != BaselineDml::ProgramDefinition::dtFloat32 ||
	    outputType != BaselineDml::ProgramDefinition::dtFloat32 ||
	    program_->inputInfo(0).dataBytes != count_ * sizeof(float) ||
	    program_->outputInfo().dataBytes != width_ * height_ * sizeof(float))
		throw std::invalid_argument("Landscape engine tensor contract mismatch");
	current_ =
		buffer(d, program_->inputInfo(0).dataBytes, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_HEAP_TYPE_DEFAULT);
	readback_ = buffer(d, program_->outputInfo().dataBytes, D3D12_RESOURCE_STATE_COPY_DEST,
			   gpuOutput_ ? D3D12_HEAP_TYPE_DEFAULT : D3D12_HEAP_TYPE_READBACK);
	check(d->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator_)),
	      "Create engine allocator");
	check(d->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator_.Get(), nullptr,
				   IID_PPV_ARGS(&commands_)),
	      "Create engine commands");
	check(commands_->Close(), "Close initial engine commands");
	check(d->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&completion_)), "Create engine completion fence");
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.NumDescriptors = 1;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	check(d->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptors_)), "Create engine descriptor heap");
	D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0};
	std::array<D3D12_ROOT_PARAMETER, 3> parameters{};
	parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	parameters[0].DescriptorTable = {1, &range};
	parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
	parameters[1].Descriptor = {0, 0};
	parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	parameters[2].Constants = {0, 0, 2};
	D3D12_ROOT_SIGNATURE_DESC rootDesc{3, parameters.data(), 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE};
	ComPtr<ID3DBlob> serialized, errors, code;
	check(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors),
	      "Serialize engine root signature");
	check(d->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
				     IID_PPV_ARGS(&root_)),
	      "Create engine root signature");
	auto hr = D3DCompile(packSource, sizeof(packSource) - 1, "Landscape RGB packing", nullptr, nullptr, "PackRgb",
			     "cs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
	if (FAILED(hr))
		throw std::runtime_error(errors ? std::string(static_cast<const char *>(errors->GetBufferPointer()),
							      errors->GetBufferSize())
						: "Engine packing shader compilation failed");
	D3D12_COMPUTE_PIPELINE_STATE_DESC state{};
	state.pRootSignature = root_.Get();
	state.CS = {code->GetBufferPointer(), code->GetBufferSize()};
	check(d->CreateComputePipelineState(&state, IID_PPV_ARGS(&pack_)), "Create engine packing pipeline");
	settings_ = buffer(d, 256, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD);
	const D3D12_RANGE noRead{0, 0};
	check(settings_->Map(0, &noRead, &settingsData_), "Map mask settings");
	setMaskSettings(0.5F);
	if (gpuOutput_) {
		D3D12_HEAP_PROPERTIES heap{};
		heap.Type = D3D12_HEAP_TYPE_DEFAULT;
		D3D12_RESOURCE_DESC desc{};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Width = width_;
		desc.Height = height_;
		desc.DepthOrArraySize = desc.MipLevels = desc.SampleDesc.Count = 1;
		desc.Format = DXGI_FORMAT_R32_FLOAT;
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET |
			     D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;
		check(d->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_SHARED, &desc, D3D12_RESOURCE_STATE_COMMON,
						 nullptr, IID_PPV_ARGS(&mask_)),
		      "Create shared model mask");
		auto handle = sharedHandle(d, mask_.Get());
		check(producer_->OpenSharedResource1(handle.get(), IID_PPV_ARGS(&mask11_)), "Open model mask");
		ComPtr<ID3D11RenderTargetView> target;
		check(producer_->CreateRenderTargetView(mask11_.Get(), nullptr, &target), "Create initial mask target");
		const float background[]{1, 1, 1, 1};
		context_->ClearRenderTargetView(target.Get(), background);
		auto completionHandle = sharedHandle(d, completion_.Get());
		check(producer_->OpenSharedFence(completionHandle.get(), IID_PPV_ARGS(&completion11_)),
		      "Open output completion fence");
#ifdef MPLD10_GPU_DIAGNOSTICS
		D3D12_QUERY_HEAP_DESC timingDesc{};
		timingDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
		timingDesc.Count = 5;
		check(d->CreateQueryHeap(&timingDesc, IID_PPV_ARGS(&timingQueries_)), "Create timing queries");
		timingReadback_ =
			buffer(d, 5 * sizeof(UINT64), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_HEAP_TYPE_READBACK);
		check(q->GetTimestampFrequency(&timingFrequency_), "Get timing frequency");
		if (!timingFrequency_ || !QueryPerformanceFrequency(&timingCpuFrequency_))
			throw std::runtime_error("Invalid timing frequency");
#endif
	}
}

auto MPLD10Engine::Impl::isDone(bool &done) const noexcept -> HRESULT
{
	done = false;
	if (unusable_)
		return E_UNEXPECTED;
	if (HRESULT hr = device_->GetDeviceRemovedReason(); FAILED(hr))
		return hr;
	const auto completed = completion_->GetCompletedValue();
	if (completed == UINT64_MAX)
		return DXGI_ERROR_DEVICE_REMOVED;
	done = hasSubmission_ && completed >= fenceValue_;
	return S_OK;
}

MPLD10Engine::Impl::~Impl() noexcept
{
	if (unusable_) {
		const uint64_t ready = readyValue_ + 1;
		if (SUCCEEDED(context_->Signal(ready11_.Get(), ready))) {
			readyValue_ = ready;
		}
		const uint64_t completion = fenceValue_ + 1;
		if (SUCCEEDED(queue_->Signal(completion_.Get(), completion))) {
			fenceValue_ = completion;
		}
	}
	if (unusable_ || readyValue_ != 0)
		context_->Flush();
	if (FAILED(blockCpuUntilFenceValueReached(completion_.Get(), fenceValue_, event_.get()))) {
		OutputDebugStringA("DirectML completion-fence wait failed during destruction.\n");
	}
	if (FAILED(blockCpuUntilFenceValueReached(ready_.Get(), readyValue_, event_.get()))) {
		OutputDebugStringA("DirectML input-ready fence wait failed during destruction.\n");
	}
#ifdef MPLD10_GPU_DIAGNOSTICS
	collectGpuTimings();
#endif
	if (settingsData_) {
		const D3D12_RANGE noWrite{0, 0};
		settings_->Unmap(0, &noWrite);
	}
}

void MPLD10Engine::Impl::setMaskSettings(float threshold) noexcept
{
	std::memcpy(settingsData_, &threshold, sizeof(threshold));
}

void MPLD10Engine::Impl::recordMaskCopy() noexcept
{
	auto *c = commands_.Get();
#ifdef MPLD10_GPU_DIAGNOSTICS
	if (timingQueries_)
		c->EndQuery(timingQueries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 3);
#endif
	transition(c, readback_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
	transition(c, mask_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
	D3D12_TEXTURE_COPY_LOCATION source{};
	source.pResource = readback_.Get();
	source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	const auto pitch = width_ * sizeof(float);
	source.PlacedFootprint.Footprint = {DXGI_FORMAT_R32_FLOAT, width_, height_, 1, static_cast<UINT>(pitch)};
	D3D12_TEXTURE_COPY_LOCATION destination{};
	destination.pResource = mask_.Get();
	destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	const D3D12_BOX box{0, 0, 0, width_, height_, 1};
	c->CopyTextureRegion(&destination, 0, 0, 0, &source, &box);
	transition(c, mask_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
	transition(c, readback_.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
#ifdef MPLD10_GPU_DIAGNOSTICS
	if (timingQueries_) {
		c->EndQuery(timingQueries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 4);
		c->ResolveQueryData(timingQueries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 5, timingReadback_.Get(), 0);
	}
#endif
}

auto MPLD10Engine::Impl::record() noexcept -> HRESULT
{
	if (HRESULT hr = allocator_->Reset(); FAILED(hr))
		return hr;
	if (HRESULT hr = commands_->Reset(allocator_.Get(), pack_.Get()); FAILED(hr))
		return hr;
#ifdef MPLD10_GPU_DIAGNOSTICS
	if (timingQueries_)
		commands_->EndQuery(timingQueries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
#endif
	ID3D12DescriptorHeap *heaps[]{descriptors_.Get()};
	const uint32_t constants[]{width_, count_};
	transition(commands_.Get(), texture_.Get(), D3D12_RESOURCE_STATE_COMMON,
		   D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	transition(commands_.Get(), current_.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
		   D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	commands_->SetDescriptorHeaps(1, heaps);
	commands_->SetComputeRootSignature(root_.Get());
	commands_->SetComputeRootDescriptorTable(0, descriptors_->GetGPUDescriptorHandleForHeapStart());
	commands_->SetComputeRootUnorderedAccessView(1, current_->GetGPUVirtualAddress());
	commands_->SetComputeRoot32BitConstants(2, 2, constants, 0);
	commands_->SetPipelineState(pack_.Get());
	commands_->Dispatch((count_ + 255) / 256, 1, 1);
	transition(commands_.Get(), texture_.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		   D3D12_RESOURCE_STATE_COMMON);
	transition(commands_.Get(), current_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		   D3D12_RESOURCE_STATE_COPY_SOURCE);
#ifdef MPLD10_GPU_DIAGNOSTICS
	if (timingQueries_)
		commands_->EndQuery(timingQueries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);
#endif
	const std::array<BaselineDml::GpuInput, 2> inputs{{
		{current_.Get(), 0, program_->inputInfo(0).dataBytes},
		{settings_.Get(), 0, sizeof(float)},
	}};
	// The shared Runtime still uses exceptions; contain them at this boundary.
	try {
		program_->recordGpuRun(commands_.Get(), inputs, nullptr, readback_.Get(), 0);
	} catch (const std::bad_alloc &) {
		return E_OUTOFMEMORY;
	} catch (const std::invalid_argument &) {
		return E_INVALIDARG;
	} catch (...) {
		return E_FAIL;
	}
#ifdef MPLD10_GPU_DIAGNOSTICS
	if (timingQueries_)
		commands_->EndQuery(timingQueries_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 2);
#endif
	if (gpuOutput_)
		recordMaskCopy();
	return commands_->Close();
}

auto MPLD10Engine::Impl::createSharedInputTexture(const D3D11_TEXTURE2D_DESC &input,
						  ComPtr<ID3D12Resource> &sharedTexture) noexcept -> HRESULT
{
	DXGI_FORMAT viewFormat = input.Format;
	switch (input.Format) {
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
		viewFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
		break;
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
		viewFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		break;
	case DXGI_FORMAT_R16G16B16A16_TYPELESS:
		viewFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
		break;
	case DXGI_FORMAT_R32G32B32A32_TYPELESS:
		viewFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
		break;
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
	case DXGI_FORMAT_R10G10B10A2_UNORM:
		break;
	default:
		return E_INVALIDARG;
	}

	D3D12_HEAP_PROPERTIES heap{};
	heap.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_DESC desc{};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Width = input.Width;
	desc.Height = input.Height;
	desc.DepthOrArraySize = input.ArraySize;
	desc.MipLevels = input.MipLevels;
	desc.SampleDesc = input.SampleDesc;
	desc.Format = input.Format;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;
	if (HRESULT hr = device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_SHARED, &desc,
							  D3D12_RESOURCE_STATE_COMMON, nullptr,
							  IID_PPV_ARGS(&sharedTexture));
	    FAILED(hr))
		return hr;
	D3D12_SHADER_RESOURCE_VIEW_DESC view{};
	view.Format = viewFormat;
	view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	view.Texture2D.MipLevels = 1;
	device_->CreateShaderResourceView(sharedTexture.Get(), &view,
					  descriptors_->GetCPUDescriptorHandleForHeapStart());
	return S_OK;
}

auto MPLD10Engine::Impl::openSharedInputTexture(ID3D12Resource *texture, ComPtr<ID3D11Texture2D> &destination) noexcept
	-> HRESULT
{
	HANDLE handle = nullptr;
	if (HRESULT hr = device_->CreateSharedHandle(texture, nullptr, GENERIC_ALL, nullptr, &handle); FAILED(hr))
		return hr;
	std::unique_ptr<void, void (*)(void *)> shared{handle, closeHandle};
	return producer_->OpenSharedResource1(shared.get(), IID_PPV_ARGS(&destination));
}

auto MPLD10Engine::Impl::runAsync(ID3D11Texture2D *input, const D3D11_TEXTURE2D_DESC &inputDesc,
				  float refineThreshold) noexcept -> HRESULT
{
#ifdef MPLD10_GPU_DIAGNOSTICS
	GUID frameActivity{};
	EventActivityIdControl(EVENT_ACTIVITY_CTRL_GET_ID, &frameActivity);
#endif
	TraceLoggingFunction(g_hProvider, TraceLoggingKeyword(0x1));
	if (unusable_) {
		needsRecreation_ = true;
		return E_UNEXPECTED;
	}
	if (hasSubmission_)
		return E_PENDING;
	if (HRESULT hr = device_->GetDeviceRemovedReason(); FAILED(hr)) {
		needsRecreation_ = true;
		return hr;
	}
	if (inputDesc.MipLevels != 1 || inputDesc.ArraySize != 1 || inputDesc.SampleDesc.Count != 1)
		return E_INVALIDARG;
	if (inputFormat_ != DXGI_FORMAT_UNKNOWN && inputFormat_ != inputDesc.Format) {
		needsRecreation_ = true;
		return E_INVALIDARG;
	}
	if (inputFormat_ == DXGI_FORMAT_UNKNOWN) {
		HRESULT hr = createSharedInputTexture(inputDesc, texture_);
		if (SUCCEEDED(hr))
			hr = openSharedInputTexture(texture_.Get(), destination_);
		if (SUCCEEDED(hr))
			hr = record();
		if (FAILED(hr)) {
			destination_.Reset();
			texture_.Reset();
			return hr;
		}
		inputFormat_ = inputDesc.Format;
	}
#ifdef MPLD10_GPU_DIAGNOSTICS
	collectGpuTimings();
	timingFrameActivity_ = frameActivity;
#endif
	setMaskSettings(refineThreshold);
	{
		TraceLoggingThreadActivity<g_hProvider, 0x1> trace;
		TraceLoggingWriteStart(trace, "MPLD10Engine.Copy11To12");
		context_->CopyResource(destination_.Get(), input);
		const uint64_t nextReadyValue = readyValue_ + 1;
		if (HRESULT hr = context_->Signal(ready11_.Get(), nextReadyValue); FAILED(hr)) {
			unusable_ = true;
			needsRecreation_ = true;
			return hr;
		}
		readyValue_ = nextReadyValue;
		if (HRESULT hr = queue_->Wait(ready_.Get(), nextReadyValue); FAILED(hr)) {
			unusable_ = true;
			needsRecreation_ = true;
			return hr;
		}
	}
	{
		TraceLoggingThreadActivity<g_hProvider, 0x1> trace;
		TraceLoggingWriteStart(trace, "MPLD10Engine.Inference");
		ID3D12CommandList *lists[]{commands_.Get()};
#ifdef MPLD10_GPU_DIAGNOSTICS
		if (timingQueries_) {
			if (HRESULT hr = queue_->GetClockCalibration(&timingCalibrationGpu_, &timingCalibrationCpu_);
			    FAILED(hr)) {
				unusable_ = needsRecreation_ = true;
				return hr;
			}
			QueryPerformanceCounter(&timingSubmitted_);
		}
#endif
		queue_->ExecuteCommandLists(1, lists);
		const uint64_t next = fenceValue_ + 1;
		if (HRESULT hr = queue_->Signal(completion_.Get(), next); FAILED(hr)) {
			unusable_ = true;
			needsRecreation_ = true;
			return hr;
		}
		fenceValue_ = next;
		hasSubmission_ = true;
	}
	return S_OK;
}

auto MPLD10Engine::Impl::ensureOutput() noexcept -> HRESULT
{
	if (!gpuOutput_ || unusable_)
		return E_UNEXPECTED;
	if (!hasSubmission_)
		return S_OK;
	if (HRESULT hr = context_->Wait(completion11_.Get(), fenceValue_); FAILED(hr)) {
		unusable_ = true;
		return hr;
	}
	hasSubmission_ = false;
	return S_OK;
}

#ifdef MPLD10_GPU_DIAGNOSTICS
void MPLD10Engine::Impl::collectGpuTimings() noexcept
{
	if (!timingQueries_ || !fenceValue_ || timingReportedValue_ == fenceValue_)
		return;
	const auto completedValue = completion_->GetCompletedValue();
	if (completedValue == UINT64_MAX || completedValue < fenceValue_)
		return;
	LARGE_INTEGER completed;
	QueryPerformanceCounter(&completed);
	const D3D12_RANGE range{0, 5 * sizeof(UINT64)};
	void *mapped = nullptr;
	if (HRESULT hr = timingReadback_->Map(0, &range, &mapped); FAILED(hr)) {
		TraceLoggingWrite(g_hProvider, "MPLD10Engine.GpuTimingError", TraceLoggingKeyword(0x1),
				  TraceLoggingHResult(hr));
		return;
	}
	std::array<UINT64, 5> ticks{};
	std::memcpy(ticks.data(), mapped, sizeof(ticks));
	const D3D12_RANGE noWrite{0, 0};
	timingReadback_->Unmap(0, &noWrite);
	timingReportedValue_ = fenceValue_;
	const bool valid = ticks[0] && std::is_sorted(ticks.begin(), ticks.end());
	const double scale = 1000.0 / static_cast<double>(timingFrequency_);
	const double startQpc = static_cast<double>(timingCalibrationCpu_) +
				(static_cast<double>(ticks[0]) - static_cast<double>(timingCalibrationGpu_)) *
					static_cast<double>(timingCpuFrequency_.QuadPart) /
					static_cast<double>(timingFrequency_);
	TraceLoggingWrite(
		g_hProvider, "MPLD10Engine.GpuTimings", TraceLoggingKeyword(0x1),
		TraceLoggingGuid(timingFrameActivity_, "FrameActivity"), TraceLoggingBool(valid, "Valid"),
		TraceLoggingUInt64(fenceValue_, "Submission"),
		TraceLoggingFloat64((startQpc - timingSubmitted_.QuadPart) * 1000.0 / timingCpuFrequency_.QuadPart,
				    "StartDelayMs"),
		TraceLoggingFloat64((ticks[1] - ticks[0]) * scale, "PackMs"),
		TraceLoggingFloat64((ticks[2] - ticks[1]) * scale, "ModelMs"),
		TraceLoggingFloat64((ticks[3] - ticks[2]) * scale, "BetweenListsMs"),
		TraceLoggingFloat64((ticks[4] - ticks[3]) * scale, "MaskCopyMs"),
		TraceLoggingFloat64((ticks[4] - ticks[0]) * scale, "TotalGpuMs"), TraceLoggingUInt64(ticks[0], "T0"),
		TraceLoggingUInt64(ticks[1], "T1"), TraceLoggingUInt64(ticks[2], "T2"),
		TraceLoggingUInt64(ticks[3], "T3"), TraceLoggingUInt64(ticks[4], "T4"),
		TraceLoggingUInt64(timingFrequency_, "TimestampHz"),
		TraceLoggingUInt64(timingCalibrationGpu_, "CalibrationGpu"),
		TraceLoggingUInt64(timingCalibrationCpu_, "CalibrationCpuQpc"),
		TraceLoggingUInt64(timingSubmitted_.QuadPart, "SubmitQpc"),
		TraceLoggingUInt64(completed.QuadPart, "CompleteQpc"),
		TraceLoggingUInt64(timingCpuFrequency_.QuadPart, "QpcHz"));
}
#endif

auto MPLD10Engine::Impl::getResult(std::span<float> output) noexcept -> HRESULT
{
	if (gpuOutput_)
		return E_UNEXPECTED;
	if (output.size() != width_ * height_)
		return E_INVALIDARG;
	bool done = false;
	if (HRESULT hr = isDone(done); FAILED(hr))
		return hr;
	if (!done)
		return E_PENDING;
	TraceLoggingFunction(g_hProvider, TraceLoggingKeyword(0x1));
	const D3D12_RANGE range{0, program_->outputInfo().dataBytes};
	void *mapped = nullptr;
	if (HRESULT hr = readback_->Map(0, &range, &mapped); FAILED(hr))
		return hr;
	std::memcpy(output.data(), mapped, output.size_bytes());
	const D3D12_RANGE noWrite{0, 0};
	readback_->Unmap(0, &noWrite);
	hasSubmission_ = false;
	return S_OK;
}

} // namespace BackgroundRemoval

namespace BackgroundRemoval {

MPLD10Engine::MPLD10Engine(ID3D11Device *device, Output output)
{
	if (!device)
		throw std::invalid_argument("MPLD10Engine requires a D3D11 device");
	impl_ = std::make_unique<Impl>(device, output == Output::GpuMask);
}

MPLD10Engine::~MPLD10Engine() noexcept = default;

auto MPLD10Engine::runAsync(ID3D11Texture2D *inputTexture, float refineThreshold) noexcept -> HRESULT
{
	impl_->needsRecreation_ = false;
	if (!std::isfinite(refineThreshold) || refineThreshold < 0.0F || refineThreshold > 1.0F || !inputTexture)
		return E_INVALIDARG;
	D3D11_TEXTURE2D_DESC inputDesc{};
	inputTexture->GetDesc(&inputDesc);
	if (inputDesc.Width != Impl::width_ || inputDesc.Height != Impl::height_)
		return E_INVALIDARG;
	return impl_->runAsync(inputTexture, inputDesc, refineThreshold);
}

auto MPLD10Engine::needsRecreation() const noexcept -> bool
{
	return impl_->needsRecreation_;
}

auto MPLD10Engine::isDone(bool &done) const noexcept -> HRESULT
{
	return impl_->isDone(done);
}

auto MPLD10Engine::getResult(std::span<float> output) noexcept -> HRESULT
{
	return impl_->getResult(output);
}

auto MPLD10Engine::gpuOutput() const noexcept -> ID3D12Resource *
{
	return impl_->gpuOutput_ ? impl_->readback_.Get() : nullptr;
}
auto MPLD10Engine::device() const noexcept -> ID3D12Device *
{
	return impl_->device_.Get();
}
auto MPLD10Engine::commandQueue() const noexcept -> ID3D12CommandQueue *
{
	return impl_->queue_.Get();
}
auto MPLD10Engine::maskTexture() const noexcept -> ID3D11Texture2D *
{
	return impl_->mask11_.Get();
}

auto MPLD10Engine::ensureOutput() noexcept -> HRESULT
{
	return impl_->ensureOutput();
}

auto MPLD10Engine::blockCpuUntilInferenceCompletes() noexcept -> HRESULT
{
	bool done = false;
	if (HRESULT hr = impl_->isDone(done); FAILED(hr))
		return hr;
	if (done)
		return S_OK;
	if (!impl_->hasSubmission_)
		return E_UNEXPECTED;
	return blockCpuUntilFenceValueReached(impl_->completion_.Get(), impl_->fenceValue_, impl_->event_.get());
}

} // namespace BackgroundRemoval
