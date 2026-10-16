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
#include <algorithm>
#include <cassert>
#include <cstring>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

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
RWTexture2D<float> mask : register(u1);
RWByteAddressBuffer coarse : register(u2);
RWByteAddressBuffer horizontal : register(u3);
RWByteAddressBuffer vertical : register(u4);
RWByteAddressBuffer reference : register(u5);
RWStructuredBuffer<uint> rowSums : register(u6);
RWByteAddressBuffer decision : register(u7);
cbuffer GateSettings : register(b2) { uint sumThreshold; uint gateEnabled; }
groupshared uint sums[256];
float Luma(float3 rgb) { return dot(rgb,float3(0.2126,0.7152,0.0722)); }
[numthreads(256,1,1)]
void SumDifference(uint3 group : SV_GroupID, uint lane : SV_GroupIndex) {
 uint p=group.x*256+lane;
 float3 rgb=image.Load(int3(lane,group.x,0)).rgb;
 uint old=reference.Load(p*4);
 float3 previous=float3(old&255,(old>>8)&255,(old>>16)&255)/255.0;
 sums[lane]=(uint)round(saturate(abs(Luma(rgb)-Luma(previous)))*255.0);
 uint3 bytes=(uint3)round(saturate(rgb)*255.0);
 reference.Store(p*4,bytes.x|(bytes.y<<8)|(bytes.z<<16));
 GroupMemoryBarrierWithGroupSync();
 for(uint stride=128;stride>0;stride>>=1) {
  if(lane<stride) sums[lane]+=sums[lane+stride];
  GroupMemoryBarrierWithGroupSync();
 }
 if(lane==0) rowSums[group.x]=sums[0];
}
[numthreads(256,1,1)]
void DecideInference(uint lane : SV_GroupIndex) {
 // Root UAVs do not carry a bounds range: keep even speculative loads in range.
 sums[lane]=lane<144 ? rowSums[min(lane,143u)] : 0;
 GroupMemoryBarrierWithGroupSync();
 for(uint stride=128;stride>0;stride>>=1) {
  if(lane<stride) sums[lane]+=sums[lane+stride];
  GroupMemoryBarrierWithGroupSync();
 }
 if(lane==0) {
  uint infer=gateEnabled==0 || (sums[0]!=0 && sums[0]>=sumThreshold);
  decision.Store(0,infer);
  decision.Store(4,0);
  decision.Store(8,sums[0]);
  decision.Store(12,decision.Load(12)+infer);
 }
}
cbuffer MaskSettings : register(b1) {
 uint outputWidth; uint outputHeight; float refineThreshold; uint padding;
}
static const uint weights[9] = {4,13,30,51,60,51,30,13,4};
[numthreads(256,1,1)]
void PackRgb(uint3 id : SV_DispatchThreadID) {
 if(id.x<count) {
  uint planeSize=count/3;
  uint pixel=id.x%planeSize;
  float4 rgba=image.Load(int3(pixel%width,pixel/width,0));
  output[id.x]=rgba[id.x/planeSize];
 }
}
uint ReadCoarse(uint p) { return (coarse.Load(p & ~3u) >> ((p & 3u)*8)) & 255u; }
uint ReadHorizontal(uint p) { return (horizontal.Load(p & ~3u) >> ((p & 3u)*8)) & 255u; }
uint ReadVertical(uint p) { return (vertical.Load(p & ~3u) >> ((p & 3u)*8)) & 255u; }
[numthreads(16,16,1)]
void WriteCoarse(uint3 id : SV_DispatchThreadID) {
 uint x=id.x*4;
 if(x>=width || id.y>=count/(3*width)) return;
 uint packed=0;
 for(uint c=0;c<4;++c) {
  uint p=id.y*width+x+c;
  uint v=(uint)round(saturate(1.0-output[p])*255.0);
  packed |= v << (c*8);
 }
 coarse.Store(id.y*width+x,packed);
}
[numthreads(16,16,1)]
void BlurHorizontal(uint3 id : SV_DispatchThreadID) {
 uint x=id.x*4;
 if(x>=width || id.y>=count/(3*width)) return;
 uint packed=0;
 for(uint c=0;c<4;++c) {
  uint sum=0;
  for(int k=-4;k<=4;++k) {
   uint sx=(uint)clamp((int)(x+c)+k,0,(int)width-1);
   uint v=ReadCoarse(id.y*width+sx);
   sum+=weights[k+4]*(v/255.0 > 1.0-refineThreshold ? 255u : 0u);
  }
  packed |= ((sum+128)/256) << (c*8);
 }
 horizontal.Store(id.y*width+x,packed);
}
[numthreads(16,16,1)]
void BlurVertical(uint3 id : SV_DispatchThreadID) {
 uint x=id.x*4,height=count/(3*width);
 if(x>=width || id.y>=height) return;
 uint packed=0;
 for(uint c=0;c<4;++c) {
  uint sum=0;
  for(int k=-4;k<=4;++k) {
   uint sy=(uint)clamp((int)id.y+k,0,(int)height-1);
   sum+=weights[k+4]*ReadHorizontal(sy*width+x+c);
  }
  packed |= ((sum+128)/256) << (c*8);
 }
 vertical.Store(id.y*width+x,packed);
}
uint MaskValue(int2 p, bool refined) {
 uint height=count/(3*width);
 p=clamp(p,int2(0,0),int2(width-1,height-1));
 return refined ? ReadVertical(p.y*width+p.x) : ReadCoarse(p.y*width+p.x);
}
float ResizedMask(uint2 id, bool refined) {
 float2 p=(float2(id)+0.5)*float2(width,count/(3*width))/float2(outputWidth,outputHeight)-0.5;
 int2 base=(int2)floor(p);
 float2 f=frac(p);
 return lerp(lerp((float)MaskValue(base,refined),(float)MaskValue(base+int2(1,0),refined),f.x),
              lerp((float)MaskValue(base+int2(0,1),refined),(float)MaskValue(base+int2(1,1),refined),f.x),f.y)/255.0;
}
[numthreads(16,16,1)]
void WriteMaskRaw(uint3 id : SV_DispatchThreadID) {
 if(id.x>=outputWidth || id.y>=outputHeight) return;
 mask[id.xy]=ResizedMask(id.xy,false);
}
[numthreads(16,16,1)]
void WriteMaskRefined(uint3 id : SV_DispatchThreadID) {
 if(id.x>=outputWidth || id.y>=outputHeight) return;
 mask[id.xy]=ResizedMask(id.xy,true)>0.5 ? 1.0 : 0.0;
}
)hlsl";

} // namespace BackgroundRemoval::Detail

namespace BackgroundRemoval {
using namespace Detail;

class MediaPipeLandscapeDirectMLEngine::Impl {
public:
	static constexpr uint32_t width_ = 256, height_ = 144, count_ = width_ * height_ * 3;
	static_assert(width_ % 4 == 0);
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
	bool gpuOutput_ = false;
	bool hasSubmission_ = false;
	bool unusable_ = false;
	bool needsRecreation_ = false;
	ComPtr<ID3D12Resource> mask_;
	ComPtr<ID3D11Texture2D> mask11_;
	ComPtr<ID3D11Fence> completion11_;
	ComPtr<ID3D12PipelineState> writeMask_, writeRawMask_, writeCoarse_, blurHorizontal_, blurVertical_;
	ComPtr<ID3D12Resource> coarse_, horizontal_, vertical_, settings_;
	void *settingsData_ = nullptr;
	uint32_t outputWidth_, outputHeight_;
	bool hasModelResult_ = false;
	ComPtr<ID3D12CommandAllocator> postAllocator_, rawPostAllocator_;
	ComPtr<ID3D12GraphicsCommandList> postCommands_, rawPostCommands_;
	std::unique_ptr<void, void (*)(void *)> event_{CreateEventW(nullptr, FALSE, FALSE, nullptr), closeHandle};
	ComPtr<ID3D12CommandAllocator> allocator_;
	ComPtr<ID3D12GraphicsCommandList> commands_;
	ComPtr<ID3D12CommandAllocator> gateAllocator_;
	ComPtr<ID3D12GraphicsCommandList> gateCommands_;
	bool submittedInference_ = false;
	bool submittedPostprocess_ = false;
	ComPtr<ID3D12DescriptorHeap> descriptors_;
	ComPtr<ID3D12RootSignature> root_;
	ComPtr<ID3D12PipelineState> pack_;
	ComPtr<ID3D12PipelineState> sumDifference_, decideInference_;
	ComPtr<ID3D12Resource> reference_, rowSums_, decision_, gateSettings_, gateReadback_;
	void *gateSettingsData_ = nullptr;
	GateResult gateResult_{};
	bool submissionHasGate_ = false;
	bool lastRefineEnabled_ = false, submittedRefineEnabled_ = false;
	float lastRefineThreshold_ = 0.5F, submittedRefineThreshold_ = 0.5F;
	ComPtr<ID3D12QueryHeap> statistics_;
	ComPtr<ID3D12Resource> statisticsReadback_;
	auto readGateResult() noexcept -> HRESULT;
	Impl(ID3D11Device *native, bool gpuOutput, uint32_t outputWidth, uint32_t outputHeight, bool collectStatistics);
	auto recordPostprocess(bool refineEnabled) noexcept -> HRESULT;
	auto refineAsync(bool refineEnabled, float refineThreshold) noexcept -> HRESULT;
	void setMaskSettings(float refineThreshold) noexcept;
	auto createSharedInputTexture(const D3D11_TEXTURE2D_DESC &input, ComPtr<ID3D12Resource> &texture) noexcept
		-> HRESULT;
	auto openSharedInputTexture(ID3D12Resource *texture, ComPtr<ID3D11Texture2D> &destination) noexcept -> HRESULT;

	auto isDone(bool &done) const noexcept -> HRESULT;
	auto getResult(std::span<float> output) noexcept -> HRESULT;
	~Impl() noexcept;
	auto record() noexcept -> HRESULT;
	auto runAsync(ID3D11Texture2D *input, const D3D11_TEXTURE2D_DESC &inputDesc, bool refineEnabled,
		      float refineThreshold, double sensitivity, bool inferMask) noexcept -> HRESULT;
	auto complete() noexcept -> HRESULT;
};

MediaPipeLandscapeDirectMLEngine::Impl::Impl(ID3D11Device *native, bool gpuOutput, uint32_t outputWidth,
					     uint32_t outputHeight, bool collectStatistics)
	: gpuOutput_(gpuOutput),
	  outputWidth_(outputWidth),
	  outputHeight_(outputHeight)
{
	auto *d = device_.Get();
	auto *q = queue_.Get();
	constexpr auto w = width_, h = height_;
	if (!d || !q || !w || !h || !event_ || !native || !outputWidth_ || !outputHeight_ ||
	    outputWidth_ > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION || outputHeight_ > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION)
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
	if (program_->inputCount() != 1 || inputType != BaselineDml::ProgramDefinition::dtFloat32 ||
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
	check(d->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&gateAllocator_)), "Create gate allocator");
	check(d->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, gateAllocator_.Get(), nullptr, IID_PPV_ARGS(&gateCommands_)), "Create gate commands");
	check(gateCommands_->Close(), "Close initial gate commands");
	check(d->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&completion_)), "Create engine completion fence");
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.NumDescriptors = 2;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	check(d->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptors_)), "Create engine descriptor heap");
	D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0};
	D3D12_DESCRIPTOR_RANGE maskRange{D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1, 0, 0};
	std::array<D3D12_ROOT_PARAMETER, 12> parameters{};
	parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	parameters[0].DescriptorTable = {1, &range};
	parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
	parameters[1].Descriptor = {0, 0};
	parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	parameters[2].Constants = {0, 0, 2};
	parameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	parameters[3].DescriptorTable = {1, &maskRange};
	for (uint32_t i = 4; i < 7; ++i) {
		parameters[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
		parameters[i].Descriptor = {i - 2, 0};
	}
	parameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	parameters[7].Descriptor = {1, 0};
	for (uint32_t i = 8; i < 11; ++i) {
		parameters[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
		parameters[i].Descriptor = {i - 3, 0};
	}
	parameters[11].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	parameters[11].Descriptor = {2, 0};
	D3D12_ROOT_SIGNATURE_DESC rootDesc{12, parameters.data(), 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE};
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
	for (auto [entry, pipeline] : std::array{std::pair{"SumDifference", std::addressof(sumDifference_)},
						 std::pair{"DecideInference", std::addressof(decideInference_)}}) {
		check(D3DCompile(packSource, sizeof(packSource) - 1, "Landscape GPU gate", nullptr, nullptr,
				entry, "cs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors), "Compile GPU gate");
		state.CS = {code->GetBufferPointer(), code->GetBufferSize()};
		check(d->CreateComputePipelineState(&state, IID_PPV_ARGS(pipeline->ReleaseAndGetAddressOf())), "Create GPU gate");
	}
	reference_ = buffer(d, width_ * height_ * 4, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_HEAP_TYPE_DEFAULT);
	rowSums_ = buffer(d, height_ * 4, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT);
	decision_ = buffer(d, 24, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_HEAP_TYPE_DEFAULT);
	gateReadback_ = buffer(d, 16, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_HEAP_TYPE_READBACK);
	gateSettings_ = buffer(d, 256, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD);
	const D3D12_RANGE noGateRead{0, 0};
	check(gateSettings_->Map(0, &noGateRead, &gateSettingsData_), "Map gate settings");
	// One-time initialization: black reference, zero probabilities and inference counter.
	auto zeros = buffer(d, width_ * height_ * 4, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD);
	void *zeroData = nullptr;
	check(zeros->Map(0, &noGateRead, &zeroData), "Map initial gate data");
	std::memset(zeroData, 0, width_ * height_ * 4);
	zeros->Unmap(0, nullptr);
	check(allocator_->Reset(), "Reset initialization allocator");
	check(commands_->Reset(allocator_.Get(), nullptr), "Reset initialization commands");
	commands_->CopyBufferRegion(reference_.Get(), 0, zeros.Get(), 0, width_ * height_ * 4);
	commands_->CopyBufferRegion(decision_.Get(), 0, zeros.Get(), 0, 24);
	commands_->CopyBufferRegion(readback_.Get(), 0, zeros.Get(), 0, width_ * height_ * 4);
	transition(commands_.Get(), reference_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	transition(commands_.Get(), decision_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
	check(commands_->Close(), "Close gate initialization");
	ID3D12CommandList *initialLists[]{commands_.Get()};
	queue_->ExecuteCommandLists(1, initialLists);
	check(queue_->Signal(completion_.Get(), ++fenceValue_), "Signal gate initialization");
	check(blockCpuUntilFenceValueReached(completion_.Get(), fenceValue_, event_.get()), "Initialize gate resources");
	if (collectStatistics) {
		const D3D12_QUERY_HEAP_DESC query{D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS, 1, 0};
		check(d->CreateQueryHeap(&query, IID_PPV_ARGS(&statistics_)), "Create gate diagnostic query");
		statisticsReadback_ = buffer(d, sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS),
			D3D12_RESOURCE_STATE_COPY_DEST, D3D12_HEAP_TYPE_READBACK);
	}
	if (gpuOutput_) {
		D3D12_HEAP_PROPERTIES heap{};
		heap.Type = D3D12_HEAP_TYPE_DEFAULT;
		D3D12_RESOURCE_DESC desc{};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Width = outputWidth_;
		desc.Height = outputHeight_;
		desc.DepthOrArraySize = desc.MipLevels = desc.SampleDesc.Count = 1;
		desc.Format = DXGI_FORMAT_R8_UNORM;
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
		D3D12_UNORDERED_ACCESS_VIEW_DESC view{};
		view.Format = DXGI_FORMAT_R8_UNORM;
		view.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		auto descriptor = descriptors_->GetCPUDescriptorHandleForHeapStart();
		descriptor.ptr += d->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		d->CreateUnorderedAccessView(mask_.Get(), nullptr, &view, descriptor);
		for (auto [entry, pipeline] : std::array{std::pair{"WriteMaskRefined", std::addressof(writeMask_)},
							 std::pair{"WriteMaskRaw", std::addressof(writeRawMask_)},
							 std::pair{"WriteCoarse", std::addressof(writeCoarse_)},
							 std::pair{"BlurHorizontal", std::addressof(blurHorizontal_)},
							 std::pair{"BlurVertical", std::addressof(blurVertical_)}}) {
			check(D3DCompile(packSource, sizeof(packSource) - 1, "Landscape mask refinement", nullptr,
					 nullptr, entry, "cs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors),
			      "Compile mask refinement");
			state.CS = {code->GetBufferPointer(), code->GetBufferSize()};
			check(d->CreateComputePipelineState(&state, IID_PPV_ARGS(pipeline->ReleaseAndGetAddressOf())),
			      "Create mask refinement pipeline");
		}
		for (auto *resource : {std::addressof(coarse_), std::addressof(horizontal_), std::addressof(vertical_)})
			*resource = buffer(d, width_ * height_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					   D3D12_HEAP_TYPE_DEFAULT);
		settings_ = buffer(d, 256, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD);
		const D3D12_RANGE noRead{0, 0};
		check(settings_->Map(0, &noRead, &settingsData_), "Map mask settings");
		setMaskSettings(0.5F);
		check(d->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&postAllocator_)),
		      "Create mask allocator");
		check(d->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, postAllocator_.Get(), nullptr,
					   IID_PPV_ARGS(&postCommands_)),
		      "Create mask commands");
		check(postCommands_->Close(), "Close initial mask commands");
		check(recordPostprocess(true), "Record mask refinement");
		check(d->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&rawPostAllocator_)),
		      "Create raw-mask allocator");
		check(d->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, rawPostAllocator_.Get(), nullptr,
					   IID_PPV_ARGS(&rawPostCommands_)),
		      "Create raw-mask commands");
		check(rawPostCommands_->Close(), "Close initial raw-mask commands");
		check(recordPostprocess(false), "Record raw-mask resize");
	}
}

auto MediaPipeLandscapeDirectMLEngine::Impl::isDone(bool &done) const noexcept -> HRESULT
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

MediaPipeLandscapeDirectMLEngine::Impl::~Impl() noexcept
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
	if (settingsData_) {
		const D3D12_RANGE noWrite{0, 0};
		settings_->Unmap(0, &noWrite);
	}
	if (gateSettingsData_)
		gateSettings_->Unmap(0, nullptr);
}

auto MediaPipeLandscapeDirectMLEngine::Impl::readGateResult() noexcept -> HRESULT
{
	if (!submissionHasGate_)
		return S_OK;
	void *mapped = nullptr;
	const D3D12_RANGE read{0, 16};
	if (HRESULT hr = gateReadback_->Map(0, &read, &mapped); FAILED(hr))
		return hr;
	std::array<uint32_t, 4> values{};
	std::memcpy(values.data(), mapped, 16);
	const D3D12_RANGE noWrite{0, 0};
	gateReadback_->Unmap(0, &noWrite);
	gateResult_ = {values[2], values[3], values[0] != 0};
	gateResult_.modelSubmitted = submittedInference_;
	if (statistics_) {
		const D3D12_RANGE statsRead{0, sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS)};
		if (HRESULT hr = statisticsReadback_->Map(0, &statsRead, &mapped); FAILED(hr))
			return hr;
		gateResult_.csInvocations = static_cast<const D3D12_QUERY_DATA_PIPELINE_STATISTICS *>(mapped)->CSInvocations;
		statisticsReadback_->Unmap(0, &noWrite);
	}
	return S_OK;
}

void MediaPipeLandscapeDirectMLEngine::Impl::setMaskSettings(float threshold) noexcept
{
	struct Settings {
		uint32_t width, height;
		float threshold;
		uint32_t padding;
	} settings{outputWidth_, outputHeight_, threshold, 0};
	std::memcpy(settingsData_, &settings, sizeof(settings));
}

auto MediaPipeLandscapeDirectMLEngine::Impl::recordPostprocess(bool refineEnabled) noexcept -> HRESULT
{
	auto *allocator = refineEnabled ? postAllocator_.Get() : rawPostAllocator_.Get();
	auto *c = refineEnabled ? postCommands_.Get() : rawPostCommands_.Get();
	if (HRESULT hr = allocator->Reset(); FAILED(hr))
		return hr;
	if (HRESULT hr = c->Reset(allocator, nullptr); FAILED(hr))
		return hr;
	transition(c, readback_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	transition(c, mask_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	ID3D12DescriptorHeap *heaps[]{descriptors_.Get()};
	c->SetDescriptorHeaps(1, heaps);
	c->SetComputeRootSignature(root_.Get());
	c->SetComputeRootUnorderedAccessView(1, readback_->GetGPUVirtualAddress());
	const uint32_t constants[]{width_, count_};
	c->SetComputeRoot32BitConstants(2, 2, constants, 0);
	auto descriptor = descriptors_->GetGPUDescriptorHandleForHeapStart();
	descriptor.ptr += device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	c->SetComputeRootDescriptorTable(3, descriptor);
	c->SetComputeRootUnorderedAccessView(4, coarse_->GetGPUVirtualAddress());
	c->SetComputeRootUnorderedAccessView(5, horizontal_->GetGPUVirtualAddress());
	c->SetComputeRootUnorderedAccessView(6, vertical_->GetGPUVirtualAddress());
	c->SetComputeRootConstantBufferView(7, settings_->GetGPUVirtualAddress());
	// Every node owns its output; only producer-to-consumer UAV ordering is needed.
	for (auto [pipeline, resource] : std::array{std::pair{writeCoarse_.Get(), coarse_.Get()},
						    std::pair{blurHorizontal_.Get(), horizontal_.Get()},
						    std::pair{blurVertical_.Get(), vertical_.Get()}}) {
		if (!refineEnabled && pipeline != writeCoarse_.Get())
			continue;
		c->SetPipelineState(pipeline);
		c->Dispatch((width_ / 4 + 15) / 16, (height_ + 15) / 16, 1);
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		barrier.UAV.pResource = resource;
		c->ResourceBarrier(1, &barrier);
	}
	c->SetPipelineState(refineEnabled ? writeMask_.Get() : writeRawMask_.Get());
	c->Dispatch((outputWidth_ + 15) / 16, (outputHeight_ + 15) / 16, 1);
	D3D12_RESOURCE_BARRIER reuse{};
	reuse.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	c->ResourceBarrier(1, &reuse);
	transition(c, mask_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
	transition(c, readback_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);
	return c->Close();
}

auto MediaPipeLandscapeDirectMLEngine::Impl::refineAsync(bool enabled, float threshold) noexcept -> HRESULT
{
	needsRecreation_ = false;
	if (hasSubmission_)
		return E_PENDING;
	if (!gpuOutput_ || !hasModelResult_)
		return E_UNEXPECTED;
	if (unusable_) {
		needsRecreation_ = true;
		return E_UNEXPECTED;
	}
	if (!std::isfinite(threshold) || threshold < 0.0F || threshold > 1.0F)
		return E_INVALIDARG;
	if (HRESULT hr = device_->GetDeviceRemovedReason(); FAILED(hr))
		return hr;
	setMaskSettings(threshold);
	submittedRefineEnabled_ = enabled;
	submittedRefineThreshold_ = threshold;
	// Order against all OBS reads of the shared output, also for settings-only work.
	const uint64_t ready = readyValue_ + 1;
	if (HRESULT hr = context_->Signal(ready11_.Get(), ready); FAILED(hr)) {
		unusable_ = true;
		return hr;
	}
	readyValue_ = ready;
	context_->Flush();
	if (HRESULT hr = queue_->Wait(ready_.Get(), ready); FAILED(hr)) {
		unusable_ = true;
		return hr;
	}
	ID3D12CommandList *lists[]{enabled ? postCommands_.Get() : rawPostCommands_.Get()};
	queue_->ExecuteCommandLists(1, lists);
	const uint64_t next = fenceValue_ + 1;
	if (HRESULT hr = queue_->Signal(completion_.Get(), next); FAILED(hr)) {
		unusable_ = true;
		return hr;
	}
	fenceValue_ = next;
	hasSubmission_ = true;
	submissionHasGate_ = false;
	submittedInference_ = false;
	submittedPostprocess_ = true;
	return S_OK;
}

auto MediaPipeLandscapeDirectMLEngine::Impl::record() noexcept -> HRESULT
{
	if (HRESULT hr = gateAllocator_->Reset(); FAILED(hr))
		return hr;
	if (HRESULT hr = gateCommands_->Reset(gateAllocator_.Get(), pack_.Get()); FAILED(hr))
		return hr;
	if (statistics_)
		gateCommands_->BeginQuery(statistics_.Get(), D3D12_QUERY_TYPE_PIPELINE_STATISTICS, 0);
	transition(gateCommands_.Get(), texture_.Get(), D3D12_RESOURCE_STATE_COMMON,
		   D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	ID3D12DescriptorHeap *heaps[]{descriptors_.Get()};
	gateCommands_->SetDescriptorHeaps(1, heaps);
	gateCommands_->SetComputeRootSignature(root_.Get());
	gateCommands_->SetComputeRootDescriptorTable(0, descriptors_->GetGPUDescriptorHandleForHeapStart());
	gateCommands_->SetComputeRootUnorderedAccessView(1, current_->GetGPUVirtualAddress());
	gateCommands_->SetComputeRootUnorderedAccessView(8, reference_->GetGPUVirtualAddress());
	gateCommands_->SetComputeRootUnorderedAccessView(9, rowSums_->GetGPUVirtualAddress());
	gateCommands_->SetComputeRootUnorderedAccessView(10, decision_->GetGPUVirtualAddress());
	gateCommands_->SetComputeRootConstantBufferView(11, gateSettings_->GetGPUVirtualAddress());
	const uint32_t constants[]{width_, count_};
	gateCommands_->SetComputeRoot32BitConstants(2, 2, constants, 0);
	transition(gateCommands_.Get(), decision_.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	D3D12_RESOURCE_BARRIER gateBarriers[2]{};
	for (auto &barrier : gateBarriers)
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	gateBarriers[0].UAV.pResource = reference_.Get();
	gateBarriers[1].UAV.pResource = rowSums_.Get();
	gateCommands_->ResourceBarrier(2, gateBarriers);
	gateCommands_->SetPipelineState(sumDifference_.Get());
	gateCommands_->Dispatch(height_, 1, 1);
	gateCommands_->ResourceBarrier(1, &gateBarriers[1]);
	gateCommands_->SetPipelineState(decideInference_.Get());
	gateCommands_->Dispatch(1, 1, 1);
	transition(gateCommands_.Get(), decision_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
	gateCommands_->CopyBufferRegion(gateReadback_.Get(), 0, decision_.Get(), 0, 16);
	if (statistics_) {
		gateCommands_->EndQuery(statistics_.Get(), D3D12_QUERY_TYPE_PIPELINE_STATISTICS, 0);
		gateCommands_->ResolveQueryData(statistics_.Get(), D3D12_QUERY_TYPE_PIPELINE_STATISTICS, 0, 1, statisticsReadback_.Get(), 0);
	}
	transition(gateCommands_.Get(), texture_.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);
	if (HRESULT hr = gateCommands_->Close(); FAILED(hr))
		return hr;
	if (HRESULT hr = allocator_->Reset(); FAILED(hr))
		return hr;
	if (HRESULT hr = commands_->Reset(allocator_.Get(), pack_.Get()); FAILED(hr))
		return hr;
	transition(commands_.Get(), texture_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	transition(commands_.Get(), current_.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
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
	const BaselineDml::GpuInput input{current_.Get(), 0, program_->inputInfo(0).dataBytes};
	// The shared Runtime still uses exceptions; contain them at this boundary.
	try {
		program_->recordGpuRun(commands_.Get(), std::span(&input, 1), nullptr, readback_.Get(), 0);
	} catch (const std::bad_alloc &) {
		return E_OUTOFMEMORY;
	} catch (const std::invalid_argument &) {
		return E_INVALIDARG;
	} catch (...) {
		return E_FAIL;
	}
	return commands_->Close();
}

auto MediaPipeLandscapeDirectMLEngine::Impl::createSharedInputTexture(const D3D11_TEXTURE2D_DESC &input,
								      ComPtr<ID3D12Resource> &sharedTexture) noexcept
	-> HRESULT
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

auto MediaPipeLandscapeDirectMLEngine::Impl::openSharedInputTexture(ID3D12Resource *texture,
								    ComPtr<ID3D11Texture2D> &destination) noexcept
	-> HRESULT
{
	HANDLE handle = nullptr;
	if (HRESULT hr = device_->CreateSharedHandle(texture, nullptr, GENERIC_ALL, nullptr, &handle); FAILED(hr))
		return hr;
	std::unique_ptr<void, void (*)(void *)> shared{handle, closeHandle};
	return producer_->OpenSharedResource1(shared.get(), IID_PPV_ARGS(&destination));
}

auto MediaPipeLandscapeDirectMLEngine::Impl::runAsync(ID3D11Texture2D *input, const D3D11_TEXTURE2D_DESC &inputDesc,
						      bool refineEnabled, float refineThreshold, double sensitivity, bool inferMask) noexcept -> HRESULT
{
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
	if (gpuOutput_)
		setMaskSettings(refineThreshold);
	const double limit = std::ceil(255.0 * width_ * height_ * std::pow(10.0, -sensitivity / 20.0));
	const std::array<uint32_t, 2> gateSettings{
		std::isnan(limit) ? 0u : static_cast<uint32_t>(std::clamp(limit, 0.0, 255.0 * width_ * height_ + 1)),
		std::isfinite(sensitivity) ? 1u : 0u};
	std::memcpy(gateSettingsData_, gateSettings.data(), sizeof(gateSettings));
	submittedRefineEnabled_ = refineEnabled;
	submittedRefineThreshold_ = refineThreshold;
	{
		TraceLoggingThreadActivity<g_hProvider, 0x1> trace;
		TraceLoggingWriteStart(trace, "MPLDEngine.Copy11To12");
		context_->CopyResource(destination_.Get(), input);
		const uint64_t nextReadyValue = readyValue_ + 1;
		if (HRESULT hr = context_->Signal(ready11_.Get(), nextReadyValue); FAILED(hr)) {
			unusable_ = true;
			needsRecreation_ = true;
			return hr;
		}
		readyValue_ = nextReadyValue;
		context_->Flush();
		if (HRESULT hr = queue_->Wait(ready_.Get(), nextReadyValue); FAILED(hr)) {
			unusable_ = true;
			needsRecreation_ = true;
			return hr;
		}
	}
	{
		TraceLoggingThreadActivity<g_hProvider, 0x1> trace;
		TraceLoggingWriteStart(trace, "MPLDEngine.Inference");
		ID3D12CommandList *lists[3]{gateCommands_.Get()};
		UINT listCount = 1;
		if (inferMask)
			lists[listCount++] = commands_.Get();
		submittedPostprocess_ = gpuOutput_ && (inferMask || (hasModelResult_ &&
			(refineEnabled != lastRefineEnabled_ || refineThreshold != lastRefineThreshold_)));
		if (submittedPostprocess_)
			lists[listCount++] = refineEnabled ? postCommands_.Get() : rawPostCommands_.Get();
		// Keep explicit execution boundaries between gate, DirectML and postprocess.
		// The retained-output test detects corruption when these lists are batched
		// in one ExecuteCommandLists call on the tested UHD 630 driver.
		for (UINT index = 0; index < listCount; ++index)
			queue_->ExecuteCommandLists(1, lists + index);
		submittedInference_ = inferMask;
		const uint64_t next = fenceValue_ + 1;
		if (HRESULT hr = queue_->Signal(completion_.Get(), next); FAILED(hr)) {
			unusable_ = true;
			needsRecreation_ = true;
			return hr;
		}
		fenceValue_ = next;
		hasSubmission_ = true;
		submissionHasGate_ = true;
	}
	return S_OK;
}

auto MediaPipeLandscapeDirectMLEngine::Impl::complete() noexcept -> HRESULT
{
	if (!gpuOutput_)
		return E_UNEXPECTED;
	bool done = false;
	if (HRESULT hr = isDone(done); FAILED(hr))
		return hr;
	if (!done)
		return E_PENDING;
	if (HRESULT hr = readGateResult(); FAILED(hr))
		return hr;
	if (HRESULT hr = context_->Wait(completion11_.Get(), fenceValue_); FAILED(hr)) {
		unusable_ = true;
		return hr;
	}
	hasModelResult_ = hasModelResult_ || submittedInference_;
	if (submittedPostprocess_) {
		lastRefineEnabled_ = submittedRefineEnabled_;
		lastRefineThreshold_ = submittedRefineThreshold_;
	}
	hasSubmission_ = false;
	return S_OK;
}

auto MediaPipeLandscapeDirectMLEngine::Impl::getResult(std::span<float> output) noexcept -> HRESULT
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
	if (HRESULT hr = readGateResult(); FAILED(hr))
		return hr;
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

#include <graphics/graphics.h>

namespace BackgroundRemoval {

MediaPipeLandscapeDirectMLEngine::MediaPipeLandscapeDirectMLEngine(Output output, uint32_t outputWidth,
								   uint32_t outputHeight, bool collectStatistics)
{
	if (gs_get_device_type() != GS_DEVICE_DIRECT3D_11)
		throw std::invalid_argument("MediaPipeLandscapeDirectMLEngine requires the D3D11 backend");
	auto *device = static_cast<ID3D11Device *>(gs_get_device_obj());
	if (!device)
		throw std::runtime_error("The D3D11 device is unavailable");
	impl_ = std::make_unique<Impl>(device, output == Output::GpuMask, outputWidth, outputHeight, collectStatistics);
}

MediaPipeLandscapeDirectMLEngine::~MediaPipeLandscapeDirectMLEngine() noexcept = default;

auto MediaPipeLandscapeDirectMLEngine::runAsync(gs_texture_t *inputTexture, bool refineEnabled,
						float refineThreshold, double sensitivity, bool inferMask) noexcept -> HRESULT
{
	impl_->needsRecreation_ = false;
	if (!std::isfinite(refineThreshold) || refineThreshold < 0.0F || refineThreshold > 1.0F || !inputTexture ||
	    gs_texture_get_width(inputTexture) != Impl::width_ || gs_texture_get_height(inputTexture) != Impl::height_)
		return E_INVALIDARG;
	auto *texture = static_cast<ID3D11Texture2D *>(gs_texture_get_obj(inputTexture));
	if (!texture)
		return E_INVALIDARG;
	D3D11_TEXTURE2D_DESC inputDesc{};
	texture->GetDesc(&inputDesc);
	return impl_->runAsync(texture, inputDesc, refineEnabled, refineThreshold, sensitivity, inferMask);
}

auto MediaPipeLandscapeDirectMLEngine::gateResult() const noexcept -> GateResult
{
	return impl_->gateResult_;
}

auto MediaPipeLandscapeDirectMLEngine::refineAsync(bool enabled, float threshold) noexcept -> HRESULT
{
	return impl_->refineAsync(enabled, threshold);
}

auto MediaPipeLandscapeDirectMLEngine::needsRecreation() const noexcept -> bool
{
	return impl_->needsRecreation_;
}

auto MediaPipeLandscapeDirectMLEngine::isDone(bool &done) const noexcept -> HRESULT
{
	return impl_->isDone(done);
}

auto MediaPipeLandscapeDirectMLEngine::getResult(std::span<float> output) noexcept -> HRESULT
{
	return impl_->getResult(output);
}

auto MediaPipeLandscapeDirectMLEngine::gpuOutput() const noexcept -> ID3D12Resource *
{
	return impl_->gpuOutput_ ? impl_->readback_.Get() : nullptr;
}
auto MediaPipeLandscapeDirectMLEngine::device() const noexcept -> ID3D12Device *
{
	return impl_->device_.Get();
}
auto MediaPipeLandscapeDirectMLEngine::commandQueue() const noexcept -> ID3D12CommandQueue *
{
	return impl_->queue_.Get();
}
auto MediaPipeLandscapeDirectMLEngine::maskTexture() const noexcept -> ID3D11Texture2D *
{
	return impl_->mask11_.Get();
}

auto MediaPipeLandscapeDirectMLEngine::complete() noexcept -> HRESULT
{
	return impl_->complete();
}

auto MediaPipeLandscapeDirectMLEngine::blockCpuUntilSubmittedGpuWorkCompletes() noexcept -> HRESULT
{
	return blockCpuUntilInferenceCompletes();
}

auto MediaPipeLandscapeDirectMLEngine::blockCpuUntilInferenceCompletes() noexcept -> HRESULT
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
