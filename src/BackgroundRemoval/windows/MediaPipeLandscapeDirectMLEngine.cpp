// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include "MediaPipeLandscapeDirectMLEngine.hpp"

#include <array>
#include <cassert>
#include <cstring>
#include <stdexcept>
#include <string>

#include <d3d11_4.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_4.h>
#include <TraceLoggingProvider.h>
#include <wrl/client.h>

#include <DmlProgram/BaselineDmlProgramRuntime.hpp>
#include "TraceLoggingProvider.hpp"

using Microsoft::WRL::ComPtr;

namespace BackgroundRemoval::Generated {
auto makeMediaPipeLandscapeDmlProgram() -> std::unique_ptr<BaselineDml::BaselineDmlProgramRuntime>;
}

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

	TraceLoggingThreadActivity<provider, 0x1> waiting;
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

namespace BackgroundRemoval::Detail {

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
		D3D12_RESOURCE_STATES after)
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

} // namespace BackgroundRemoval::Detail

namespace BackgroundRemoval {
using namespace Detail;

class MediaPipeLandscapeDirectMLEngine::Impl {
public:
	static constexpr uint32_t width_ = 256, height_ = 144, count_ = width_ * height_ * 3;
	DXGI_FORMAT inputFormat_ = DXGI_FORMAT_UNKNOWN;
	std::unique_ptr<BaselineDml::BaselineDmlProgramRuntime> program_{Generated::makeMediaPipeLandscapeDmlProgram()};
	ComPtr<ID3D12Device> device_{program_->device()};
	ComPtr<ID3D12CommandQueue> queue_{program_->queue()};
	ComPtr<ID3D12Resource> texture_, current_, readback_;
	ComPtr<ID3D12Fence> ready_, completion_;
	ComPtr<ID3D11Device5> producer_;
	ComPtr<ID3D11DeviceContext4> context_;
	ComPtr<ID3D11Texture2D> destination_;
	ComPtr<ID3D11Fence> ready11_;
	uint64_t readyValue_ = 0, fenceValue_ = 0;
	bool hasSubmission_ = false;
	bool unusable_ = false;
	std::unique_ptr<void, void (*)(void *)> event_{CreateEventW(nullptr, FALSE, FALSE, nullptr), closeHandle};
	ComPtr<ID3D12CommandAllocator> allocator_;
	ComPtr<ID3D12GraphicsCommandList> commands_;
	ComPtr<ID3D12DescriptorHeap> descriptors_;
	ComPtr<ID3D12RootSignature> root_;
	ComPtr<ID3D12PipelineState> pack_;
	Impl(ID3D11Device *native);
	auto createSharedInputTexture(const D3D11_TEXTURE2D_DESC &input) -> ComPtr<ID3D12Resource>;
	auto openSharedInputTexture(ID3D12Resource *texture) -> ComPtr<ID3D11Texture2D>;

	auto isDone() const -> bool;
	void getResult(std::span<float> output);
	~Impl();
	void record();
	auto runAsync(ID3D11Texture2D *input, const D3D11_TEXTURE2D_DESC &inputDesc) -> RunResult;
};

MediaPipeLandscapeDirectMLEngine::Impl::Impl(ID3D11Device *native)
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
	// OBS 29 exports native D3D11 objects in graphics.h. Use its immediate
	// context so the copy follows OBS rendering on the same command stream.
	if (!native)
		throw std::runtime_error("OBS D3D11 device is unavailable");
	check(native->QueryInterface(IID_PPV_ARGS(&producer_)), "OBS D3D11 shared-fence support");
	ComPtr<IDXGIDevice> dxgiDevice;
	check(producer_.As(&dxgiDevice), "Get OBS DXGI device");
	ComPtr<IDXGIAdapter> adapter;
	check(dxgiDevice->GetAdapter(&adapter), "Get OBS GPU adapter");
	DXGI_ADAPTER_DESC adapterDesc{};
	check(adapter->GetDesc(&adapterDesc), "Get OBS GPU adapter description");
	const auto runtimeLuid = d->GetAdapterLuid();
	if (adapterDesc.AdapterLuid.LowPart != runtimeLuid.LowPart ||
	    adapterDesc.AdapterLuid.HighPart != runtimeLuid.HighPart)
		throw std::invalid_argument("OBS and DirectML must use the same GPU adapter");
	ComPtr<ID3D11DeviceContext> nativeContext;
	producer_->GetImmediateContext(&nativeContext);
	check(nativeContext.As(&context_), "OBS D3D11 context shared-fence support");

	check(d->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&ready_)), "Create input-ready fence");
	auto readyHandle = sharedHandle(d, ready_.Get());
	check(producer_->OpenSharedFence(readyHandle.get(), IID_PPV_ARGS(&ready11_)), "Open input-ready fence");
	const auto inputType = program_->inputInfo(0).type;
	const auto outputType = program_->outputInfo().type;
	if (program_->inputCount() != 1 ||
	    inputType != BaselineDml::ProgramDefinition::dtFloat32 ||
	    outputType != BaselineDml::ProgramDefinition::dtFloat32 ||
	    program_->inputInfo(0).dataBytes != count_ * sizeof(float) ||
	    program_->outputInfo().dataBytes != width_ * height_ * sizeof(float))
		throw std::invalid_argument("Landscape engine tensor contract mismatch");
	current_ = buffer(d, program_->inputInfo(0).dataBytes, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_HEAP_TYPE_DEFAULT);
	readback_ =
		buffer(d, program_->outputInfo().dataBytes, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_HEAP_TYPE_READBACK);
	check(d->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator_)),
	      "Create engine allocator");
	check(d->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator_.Get(), nullptr,
				   IID_PPV_ARGS(&commands_)),
	      "Create engine commands");
	check(commands_->Close(), "Close initial engine commands");
	check(d->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&completion_)), "Create engine completion fence");
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
}

auto MediaPipeLandscapeDirectMLEngine::Impl::isDone() const -> bool
{
	if (unusable_) {
		throw std::runtime_error("DirectML engine is unusable after a submission failure");
	}
	check(device_->GetDeviceRemovedReason(), "Engine device removed");
	return hasSubmission_ && completion_->GetCompletedValue() >= fenceValue_;
}

MediaPipeLandscapeDirectMLEngine::Impl::~Impl()
{
	if (unusable_) {
		const uint64_t ready = readyValue_ + 1;
		if (SUCCEEDED(context_->Signal(ready11_.Get(), ready))) {
			readyValue_ = ready;
		}
		context_->Flush();
		const uint64_t completion = fenceValue_ + 1;
		if (SUCCEEDED(queue_->Signal(completion_.Get(), completion))) {
			fenceValue_ = completion;
		}
	}
	if (FAILED(blockCpuUntilFenceValueReached(completion_.Get(), fenceValue_, event_.get()))) {
		OutputDebugStringA("DirectML completion-fence wait failed during destruction.\n");
	}
	if (FAILED(blockCpuUntilFenceValueReached(ready_.Get(), readyValue_, event_.get()))) {
		OutputDebugStringA("DirectML input-ready fence wait failed during destruction.\n");
	}
}

void MediaPipeLandscapeDirectMLEngine::Impl::record()
{
	check(allocator_->Reset(), "Reset engine allocator");
	check(commands_->Reset(allocator_.Get(), pack_.Get()), "Reset engine commands");
	transition(commands_.Get(), texture_.Get(), D3D12_RESOURCE_STATE_COMMON,
		   D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	transition(commands_.Get(), current_.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
		   D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	ID3D12DescriptorHeap *heaps[]{descriptors_.Get()};
	commands_->SetDescriptorHeaps(1, heaps);
	commands_->SetComputeRootSignature(root_.Get());
	commands_->SetComputeRootDescriptorTable(0, descriptors_->GetGPUDescriptorHandleForHeapStart());
	commands_->SetComputeRootUnorderedAccessView(1, current_->GetGPUVirtualAddress());
	const uint32_t constants[]{width_, count_};
	commands_->SetComputeRoot32BitConstants(2, 2, constants, 0);
	commands_->Dispatch((count_ + 255) / 256, 1, 1);
	transition(commands_.Get(), texture_.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		   D3D12_RESOURCE_STATE_COMMON);
	transition(commands_.Get(), current_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		   D3D12_RESOURCE_STATE_COPY_SOURCE);
	const BaselineDml::GpuInput input{current_.Get(), 0, program_->inputInfo(0).dataBytes};
	program_->recordGpuRun(commands_.Get(), std::span(&input, 1), nullptr, readback_.Get(), 0);
	check(commands_->Close(), "Close engine commands");
}

auto MediaPipeLandscapeDirectMLEngine::Impl::createSharedInputTexture(const D3D11_TEXTURE2D_DESC &input)
	-> ComPtr<ID3D12Resource>
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
		throw std::invalid_argument("Input texture format has no RGB model-input conversion");
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
	ComPtr<ID3D12Resource> sharedTexture;
	check(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_SHARED, &desc, D3D12_RESOURCE_STATE_COMMON,
					       nullptr, IID_PPV_ARGS(&sharedTexture)),
	      "Create shared input texture");
	D3D12_SHADER_RESOURCE_VIEW_DESC view{};
	view.Format = viewFormat;
	view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	view.Texture2D.MipLevels = 1;
	device_->CreateShaderResourceView(sharedTexture.Get(), &view,
					  descriptors_->GetCPUDescriptorHandleForHeapStart());
	return sharedTexture;
}

auto MediaPipeLandscapeDirectMLEngine::Impl::openSharedInputTexture(ID3D12Resource *texture) -> ComPtr<ID3D11Texture2D>
{
	auto handle = sharedHandle(device_.Get(), texture);
	ComPtr<ID3D11Texture2D> destination;
	check(producer_->OpenSharedResource1(handle.get(), IID_PPV_ARGS(&destination)), "Open shared input texture");
	return destination;
}

auto MediaPipeLandscapeDirectMLEngine::Impl::runAsync(ID3D11Texture2D *input, const D3D11_TEXTURE2D_DESC &inputDesc)
	-> RunResult
{
	TraceLoggingFunction(provider, TraceLoggingKeyword(0x1));

	if (unusable_) {
		throw std::runtime_error("DirectML engine is unusable after a submission failure");
	} else if (hasSubmission_) {
		throw std::logic_error("Previous DirectML result has not been retrieved");
	}

	check(device_->GetDeviceRemovedReason(), "Engine device removed");

	if (inputDesc.MipLevels != 1) {
		throw std::invalid_argument("Model-input conversion requires one mip");
	} else if (inputDesc.ArraySize != 1) {
		throw std::invalid_argument("Model-input conversion requires one array slice");
	} else if (inputDesc.SampleDesc.Count != 1) {
		throw std::invalid_argument("Model-input conversion requires one sample");
	}

	if (inputFormat_ != DXGI_FORMAT_UNKNOWN && inputFormat_ != inputDesc.Format) {
		return RunResult::NeedsRecreation;
	}

	if (inputFormat_ == DXGI_FORMAT_UNKNOWN) {
		try {
			texture_ = createSharedInputTexture(inputDesc);
			destination_ = openSharedInputTexture(texture_.Get());
			record();
			inputFormat_ = inputDesc.Format;
		} catch (...) {
			destination_.Reset();
			texture_.Reset();
			throw;
		}
	}

	{
		TraceLoggingThreadActivity<provider, 0x1> trace;
		TraceLoggingWriteStart(trace, "MPLDEngine.Copy11To12");

		context_->CopyResource(destination_.Get(), input);
		const uint64_t nextReadyValue = readyValue_ + 1;

		if (HRESULT hr = context_->Signal(ready11_.Get(), nextReadyValue); FAILED(hr)) {
			throw std::runtime_error("Failed to signal D3D11 input readiness");
		}

		readyValue_ = nextReadyValue;
		context_->Flush();

		if (HRESULT hr = queue_->Wait(ready_.Get(), nextReadyValue); FAILED(hr)) {
			throw std::runtime_error("Failed to wait for D3D12 shared input");
		}
	}

	{
		TraceLoggingThreadActivity<provider, 0x1> trace;
		TraceLoggingWriteStart(trace, "MPLDEngine.Inference");

		ID3D12CommandList *lists[]{commands_.Get()};
		queue_->ExecuteCommandLists(1, lists);
		const uint64_t next = fenceValue_ + 1;

		if (HRESULT hr = queue_->Signal(completion_.Get(), next); FAILED(hr)) {
			throw std::runtime_error("Failed to signal engine completion");
		}

		fenceValue_ = next;
		hasSubmission_ = true;
	}

	return RunResult::Submitted;
}

void MediaPipeLandscapeDirectMLEngine::Impl::getResult(std::span<float> output)
{
	if (output.size() != width_ * height_) {
		throw std::invalid_argument("Landscape output size mismatch");
	}
	if (!isDone()) {
		throw std::logic_error("DirectML result is not complete");
	}
	TraceLoggingFunction(provider, TraceLoggingKeyword(0x1));
	const D3D12_RANGE range{0, program_->outputInfo().dataBytes};
	void *mapped = nullptr;
	check(readback_->Map(0, &range, &mapped), "Map engine output");
	std::memcpy(output.data(), mapped, output.size_bytes());
	const D3D12_RANGE noWrite{0, 0};
	readback_->Unmap(0, &noWrite);
	hasSubmission_ = false;
}

} // namespace BackgroundRemoval

#include <graphics/graphics.h>

namespace BackgroundRemoval {

MediaPipeLandscapeDirectMLEngine::MediaPipeLandscapeDirectMLEngine()
{
	if (gs_get_device_type() != GS_DEVICE_DIRECT3D_11) {
		throw std::invalid_argument("MediaPipeLandscapeDirectMLEngine requires the D3D11 backend");
	}

	auto *device = static_cast<ID3D11Device *>(gs_get_device_obj());
	if (!device) {
		throw std::runtime_error("The D3D11 device is unavailable");
	}

	impl_ = std::make_unique<Impl>(device);
}

MediaPipeLandscapeDirectMLEngine::~MediaPipeLandscapeDirectMLEngine() = default;

auto MediaPipeLandscapeDirectMLEngine::runAsync(gs_texture_t *inputTexture) -> RunResult
{
	if (!inputTexture) {
		throw std::invalid_argument("Input texture is null");
	}

	std::uint32_t inputWidth = gs_texture_get_width(inputTexture);
	std::uint32_t inputHeight = gs_texture_get_height(inputTexture);

	if (inputWidth != Impl::width_ || inputHeight != Impl::height_) {
		throw std::invalid_argument("Input texture has incorrect dimensions");
	}

	auto *texture = static_cast<ID3D11Texture2D *>(gs_texture_get_obj(inputTexture));

	if (!texture) {
		throw std::invalid_argument("Input texture has no D3D11 object");
	}
	D3D11_TEXTURE2D_DESC inputDesc{};
	texture->GetDesc(&inputDesc);
	return impl_->runAsync(texture, inputDesc);
}

auto MediaPipeLandscapeDirectMLEngine::isDone() const -> bool
{
	return impl_->isDone();
}

void MediaPipeLandscapeDirectMLEngine::getResult(std::span<float> output)
{
	impl_->getResult(output);
}

void MediaPipeLandscapeDirectMLEngine::blockCpuUntilInferenceCompletes()
{
	if (!impl_->isDone()) {
		if (!impl_->hasSubmission_) {
			throw std::logic_error("No DirectML work has been submitted");
		}
		TraceLoggingThreadActivity<provider, 0x1> waiting;
		TraceLoggingWriteStart(waiting, "Pipeline.GpuOutputWait");
		check(blockCpuUntilFenceValueReached(impl_->completion_.Get(), impl_->fenceValue_, impl_->event_.get()),
		      "CPU inference-completion wait failed");
	}
}

} // namespace BackgroundRemoval
