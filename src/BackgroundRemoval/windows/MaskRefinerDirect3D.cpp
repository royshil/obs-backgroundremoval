// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "MaskRefinerDirect3D.hpp"
#include "TraceLoggingProvider.hpp"
#include <d3d11_4.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
namespace BackgroundRemoval::MaskRefinerImplementation {
void check(HRESULT hr, const char *message)
{
	if (FAILED(hr))
		throw std::runtime_error(message);
}
void closeHandle(void *value)
{
	if (value)
		CloseHandle(value);
}
auto sharedHandle(ID3D12Device *device, ID3D12DeviceChild *object) -> std::unique_ptr<void, void (*)(void *)>
{
	HANDLE handle = nullptr;
	check(device->CreateSharedHandle(object, nullptr, GENERIC_ALL, nullptr, &handle), "Share mask resource");
	return {handle, closeHandle};
}
auto buffer(ID3D12Device *device, UINT64 bytes, D3D12_HEAP_TYPE type, D3D12_RESOURCE_STATES state)
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
	ComPtr<ID3D12Resource> resource;
	check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr,
					      IID_PPV_ARGS(&resource)),
	      "Create mask buffer");
	return resource;
}
void transition(ID3D12GraphicsCommandList *commands, ID3D12Resource *resource, D3D12_RESOURCE_STATES before,
		D3D12_RESOURCE_STATES after)
{
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition = {resource, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, before, after};
	commands->ResourceBarrier(1, &barrier);
}
void uavBarrier(ID3D12GraphicsCommandList *commands, ID3D12Resource *resource)
{
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	barrier.UAV.pResource = resource;
	commands->ResourceBarrier(1, &barrier);
}
constexpr char shader[] = R"hlsl(
RWStructuredBuffer<float> source : register(u0);
RWStructuredBuffer<float> destination : register(u1);
RWTexture2D<float> mask : register(u2);
RWByteAddressBuffer status : register(u3);
cbuffer Settings : register(b0) {
 uint inputWidth,inputHeight,outputWidth,outputHeight,stage,enabled;
 float threshold; int expansion; uint hasModel;
};
float readAt(int x,int y,uint w,uint h) {return source[clamp(y,0,int(h)-1)*w+clamp(x,0,int(w)-1)];}
[numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID) {
 bool small=stage<3;uint w=small?inputWidth:outputWidth,h=small?inputHeight:outputHeight;
 if(id.x>=w||id.y>=h)return;
 uint i=id.y*w+id.x;
 if(stage==0) {
  if(!hasModel){destination[i]=1;return;}
  float p=source[i];
  if(!isfinite(p)){uint ignored;status.InterlockedOr(0,1,ignored);p=0;}
  float background=saturate(1-p);
  destination[i]=enabled?(background>1-threshold?1:0):background;return;
 }
 if(stage==1||stage==2) {
  float sum=0,total=0;
  [unroll]for(int d=-4;d<=4;++d){float weight=exp(-float(d*d)/(2*1.7*1.7));total+=weight;
   sum+=weight*readAt(int(id.x)+(stage==1?d:0),int(id.y)+(stage==2?d:0),w,h);}
  destination[i]=sum/total;return;
 }
 if(stage==3) {
  float2 p=(float2(id.xy)+0.5)*float2(inputWidth,inputHeight)/float2(outputWidth,outputHeight)-0.5;
  int2 base=int2(floor(p));float2 f=frac(p);
  float a=lerp(readAt(base.x,base.y,inputWidth,inputHeight),readAt(base.x+1,base.y,inputWidth,inputHeight),f.x);
  float b=lerp(readAt(base.x,base.y+1,inputWidth,inputHeight),readAt(base.x+1,base.y+1,inputWidth,inputHeight),f.x);
  float value=lerp(a,b,f.y);destination[i]=enabled?(value>0.5?1:0):value;return;
 }
 if(stage==4||stage==5) {
  float value=expansion>0?1:0;int radius=abs(expansion);
  [loop]for(int d=-radius;d<=radius;++d){float v=readAt(int(id.x)+(stage==4?d:0),int(id.y)+(stage==5?d:0),w,h);
   value=expansion>0?min(value,v):max(value,v);}
  if(stage==4)destination[i]=value;else mask[id.xy]=value;return;
 }
 mask[id.xy]=source[i];
}
)hlsl";
} // namespace BackgroundRemoval::MaskRefinerImplementation
namespace BackgroundRemoval {
using namespace MaskRefinerImplementation;
class MaskRefinerDirect3D::Impl final {
public:
	ComPtr<ID3D12Device> device_;
	ComPtr<ID3D12CommandQueue> queue_;
	ComPtr<ID3D11DeviceContext4> context_;
	ComPtr<ID3D12Resource> probabilities_, smallA_, smallB_, largeA_, largeB_, output_, status_, statusReadback_,
		zero_;
	ComPtr<ID3D11Texture2D> output11_;
	ComPtr<ID3D12Fence> completion_, ready_;
	ComPtr<ID3D11Fence> completion11_, ready11_;
	ComPtr<ID3D12CommandAllocator> allocator_;
	ComPtr<ID3D12GraphicsCommandList> commands_;
	ComPtr<ID3D12RootSignature> root_;
	ComPtr<ID3D12PipelineState> pipeline_;
	ComPtr<ID3D12DescriptorHeap> descriptors_;
	std::unique_ptr<void, void (*)(void *)> event_{CreateEventW(nullptr, FALSE, FALSE, nullptr), closeHandle};
	std::uint32_t inputWidth_, inputHeight_, outputWidth_, outputHeight_;
	UINT64 fenceValue_ = 0, readyValue_ = 0;
	bool pending_ = false, hasModel_ = false, unusable_ = false;
	Impl(ID3D12Device *device, ID3D12CommandQueue *queue, ID3D11Device *graphicsDevice, std::uint32_t iw,
	     std::uint32_t ih, std::uint32_t ow, std::uint32_t oh);
	~Impl();
	void record(ID3D12Resource *input, const Settings &settings);
	void dispatch(UINT pass, ID3D12Resource *source, ID3D12Resource *destination, const Settings &settings);
	void runAsync(ID3D12Resource *input, const Settings &settings);
	auto isDone() const -> bool;
	void blockCpuUntilComplete();
	void complete();
};
MaskRefinerDirect3D::Impl::Impl(ID3D12Device *device, ID3D12CommandQueue *queue, ID3D11Device *graphicsDevice,
				std::uint32_t iw, std::uint32_t ih, std::uint32_t ow, std::uint32_t oh)
	: device_(device),
	  queue_(queue),
	  inputWidth_(iw),
	  inputHeight_(ih),
	  outputWidth_(ow),
	  outputHeight_(oh)
{
	if (!device || !queue || !graphicsDevice || !event_ || !iw || !ih || !ow || !oh || iw > 16384 || ih > 16384 ||
	    ow > 16384 || oh > 16384)
		throw std::invalid_argument("Invalid mask refiner dimensions or device");
	ComPtr<ID3D12Device> queueDevice;
	check(queue->GetDevice(IID_PPV_ARGS(&queueDevice)), "Get mask queue device");
	if (queueDevice.Get() != device || queue->GetDesc().Type != D3D12_COMMAND_LIST_TYPE_DIRECT)
		throw std::invalid_argument("Mask refiner requires the model's direct queue");
	ComPtr<ID3D11Device5> graphics;
	check(graphicsDevice->QueryInterface(IID_PPV_ARGS(&graphics)), "Mask interop requires D3D11 shared fences");
	ComPtr<ID3D11DeviceContext> context;
	graphics->GetImmediateContext(&context);
	check(context.As(&context_), "Get mask graphics context");
	D3D12_FEATURE_DATA_FORMAT_SUPPORT support{DXGI_FORMAT_R8_UNORM};
	check(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support)),
	      "Query R8 mask support");
	if (!(support.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE))
		throw std::runtime_error("Device cannot store R8 UAV masks");
	auto smallBytes = UINT64(iw) * ih * sizeof(float), largeBytes = UINT64(ow) * oh * sizeof(float);
	probabilities_ = buffer(device, smallBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	smallA_ = buffer(device, smallBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	smallB_ = buffer(device, smallBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	largeA_ = buffer(device, largeBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	largeB_ = buffer(device, largeBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	status_ = buffer(device, 4, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	statusReadback_ = buffer(device, 4, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
	zero_ = buffer(device, 4, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
	void *mapped = nullptr;
	D3D12_RANGE none{0, 0};
	check(zero_->Map(0, &none, &mapped), "Map mask zero constant");
	std::memset(mapped, 0, 4);
	zero_->Unmap(0, nullptr);
	D3D12_HEAP_PROPERTIES heap{};
	heap.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_DESC desc{};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Width = ow;
	desc.Height = oh;
	desc.DepthOrArraySize = desc.MipLevels = desc.SampleDesc.Count = 1;
	desc.Format = DXGI_FORMAT_R8_UNORM;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS |
		     D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_SHARED, &desc, D3D12_RESOURCE_STATE_COMMON,
					      nullptr, IID_PPV_ARGS(&output_)),
	      "Create shared mask texture");
	auto handle = sharedHandle(device, output_.Get());
	check(graphics->OpenSharedResource1(handle.get(), IID_PPV_ARGS(&output11_)), "Open mask texture in D3D11");
	check(device->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&completion_)),
	      "Create mask completion fence");
	auto completionHandle = sharedHandle(device, completion_.Get());
	check(graphics->OpenSharedFence(completionHandle.get(), IID_PPV_ARGS(&completion11_)),
	      "Open mask completion fence");
	check(device->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&ready_)), "Create mask readiness fence");
	auto readyHandle = sharedHandle(device, ready_.Get());
	check(graphics->OpenSharedFence(readyHandle.get(), IID_PPV_ARGS(&ready11_)), "Open mask readiness fence");
	D3D12_DESCRIPTOR_HEAP_DESC hd{};
	hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	hd.NumDescriptors = 1;
	hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	check(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&descriptors_)), "Create mask descriptors");
	D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
	uav.Format = desc.Format;
	uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	device->CreateUnorderedAccessView(output_.Get(), nullptr, &uav,
					  descriptors_->GetCPUDescriptorHandleForHeapStart());
	std::array<D3D12_ROOT_PARAMETER, 5> params{};
	params[0].ParameterType = params[1].ParameterType = params[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
	params[0].Descriptor.ShaderRegister = 0;
	params[1].Descriptor.ShaderRegister = 1;
	params[4].Descriptor.ShaderRegister = 3;
	D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2, 0, 0};
	params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[2].DescriptorTable = {1, &range};
	params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	params[3].Constants = {0, 0, 9};
	D3D12_ROOT_SIGNATURE_DESC rd{};
	rd.NumParameters = UINT(params.size());
	rd.pParameters = params.data();
	ComPtr<ID3DBlob> serialized, errors, code;
	check(D3D12SerializeRootSignature(&rd, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors),
	      "Serialize mask root signature");
	check(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
					  IID_PPV_ARGS(&root_)),
	      "Create mask root signature");
	check(D3DCompile(shader, sizeof(shader) - 1, "MaskRefinerDirect3D", nullptr, nullptr, "main", "cs_5_0",
			 D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors),
	      "Compile mask shader");
	D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};
	pd.pRootSignature = root_.Get();
	pd.CS = {code->GetBufferPointer(), code->GetBufferSize()};
	check(device->CreateComputePipelineState(&pd, IID_PPV_ARGS(&pipeline_)), "Create mask pipeline");
	check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator_)),
	      "Create mask allocator");
	check(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator_.Get(), pipeline_.Get(),
					IID_PPV_ARGS(&commands_)),
	      "Create mask command list");
	check(commands_->Close(), "Close mask command list");
}
MaskRefinerDirect3D::Impl::~Impl()
{
	if (unusable_) {
		context_->Flush();
		const auto next = fenceValue_ + 1;
		if (SUCCEEDED(queue_->Signal(completion_.Get(), next)))
			fenceValue_ = next;
	}
	if (completion_ && completion_->GetCompletedValue() < fenceValue_ &&
	    SUCCEEDED(completion_->SetEventOnCompletion(fenceValue_, event_.get())))
		WaitForSingleObject(event_.get(), INFINITE);
}
void MaskRefinerDirect3D::Impl::dispatch(UINT pass, ID3D12Resource *source, ID3D12Resource *destination,
					 const Settings &s)
{
	const UINT constants[]{inputWidth_,
			       inputHeight_,
			       outputWidth_,
			       outputHeight_,
			       pass,
			       UINT(s.enabled),
			       std::bit_cast<UINT>(s.threshold),
			       UINT(s.enabled ? s.expansion : 0),
			       UINT(hasModel_)};
	commands_->SetComputeRootUnorderedAccessView(0, source->GetGPUVirtualAddress());
	commands_->SetComputeRootUnorderedAccessView(1, destination->GetGPUVirtualAddress());
	commands_->SetComputeRoot32BitConstants(3, 9, constants, 0);
	commands_->Dispatch(((pass < 3 ? inputWidth_ : outputWidth_) + 7) / 8,
			    ((pass < 3 ? inputHeight_ : outputHeight_) + 7) / 8, 1);
	if (pass < 5)
		uavBarrier(commands_.Get(), destination);
}
void MaskRefinerDirect3D::Impl::record(ID3D12Resource *input, const Settings &s)
{
	check(allocator_->Reset(), "Reset mask allocator");
	check(commands_->Reset(allocator_.Get(), pipeline_.Get()), "Reset mask command list");
	if (input) {
		transition(commands_.Get(), input, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
		transition(commands_.Get(), probabilities_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			   D3D12_RESOURCE_STATE_COPY_DEST);
		commands_->CopyBufferRegion(probabilities_.Get(), 0, input, 0,
					    UINT64(inputWidth_) * inputHeight_ * sizeof(float));
		transition(commands_.Get(), input, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
		transition(commands_.Get(), probabilities_.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
			   D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}
	transition(commands_.Get(), status_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		   D3D12_RESOURCE_STATE_COPY_DEST);
	commands_->CopyBufferRegion(status_.Get(), 0, zero_.Get(), 0, 4);
	transition(commands_.Get(), status_.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
		   D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	transition(commands_.Get(), output_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	ID3D12DescriptorHeap *heaps[]{descriptors_.Get()};
	commands_->SetDescriptorHeaps(1, heaps);
	commands_->SetComputeRootSignature(root_.Get());
	commands_->SetComputeRootDescriptorTable(2, descriptors_->GetGPUDescriptorHandleForHeapStart());
	commands_->SetComputeRootUnorderedAccessView(4, status_->GetGPUVirtualAddress());
	dispatch(0, probabilities_.Get(), smallA_.Get(), s);
	if (s.enabled) {
		dispatch(1, smallA_.Get(), smallB_.Get(), s);
		dispatch(2, smallB_.Get(), smallA_.Get(), s);
	}
	dispatch(3, smallA_.Get(), largeA_.Get(), s);
	if (s.enabled && s.expansion != 0) {
		dispatch(4, largeA_.Get(), largeB_.Get(), s);
		dispatch(5, largeB_.Get(), largeA_.Get(), s);
	} else
		dispatch(6, largeA_.Get(), largeB_.Get(), s);
	transition(commands_.Get(), output_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
	transition(commands_.Get(), status_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		   D3D12_RESOURCE_STATE_COPY_SOURCE);
	commands_->CopyBufferRegion(statusReadback_.Get(), 0, status_.Get(), 0, 4);
	transition(commands_.Get(), status_.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
		   D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	check(commands_->Close(), "Close mask commands");
}
void MaskRefinerDirect3D::Impl::runAsync(ID3D12Resource *input, const Settings &s)
{
	if (unusable_)
		throw std::runtime_error("Mask refiner is unusable after submission failure");
	if (pending_)
		throw std::logic_error("Previous mask result has not been completed");
	if (!std::isfinite(s.threshold) || s.threshold < 0 || s.threshold > 1 || s.expansion < -30 || s.expansion > 30)
		throw std::invalid_argument("Invalid mask settings");
	if (input) {
		ComPtr<ID3D12Device> owner;
		check(input->GetDevice(IID_PPV_ARGS(&owner)), "Get mask input device");
		D3D12_HEAP_PROPERTIES heap{};
		D3D12_HEAP_FLAGS flags{};
		check(input->GetHeapProperties(&heap, &flags), "Get mask input heap");
		if (heap.Type != D3D12_HEAP_TYPE_DEFAULT)
			throw std::invalid_argument("Mask input must be a GPU default buffer");
		auto desc = input->GetDesc();
		if (owner.Get() != device_.Get() || desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER ||
		    desc.Width < UINT64(inputWidth_) * inputHeight_ * 4)
			throw std::invalid_argument("Invalid mask input buffer");
	}
	const bool priorModel = hasModel_;
	hasModel_ |= input != nullptr;
	try {
		record(input, s);
	} catch (...) {
		hasModel_ = priorModel;
		throw;
	}
	try {
		const auto ready = readyValue_ + 1;
		check(context_->Signal(ready11_.Get(), ready), "Signal mask graphics readiness");
		readyValue_ = ready;
		context_->Flush();
		check(queue_->Wait(ready_.Get(), ready), "Wait for prior mask graphics reads");
		ID3D12CommandList *lists[]{commands_.Get()};
		queue_->ExecuteCommandLists(1, lists);
		const auto next = fenceValue_ + 1;
		check(queue_->Signal(completion_.Get(), next), "Signal mask completion");
		fenceValue_ = next;
		pending_ = true;
	} catch (...) {
		unusable_ = true;
		throw;
	}
}
auto MaskRefinerDirect3D::Impl::isDone() const -> bool
{
	if (unusable_)
		throw std::runtime_error("Mask refiner is unusable");
	check(device_->GetDeviceRemovedReason(), "Mask device removed");
	return pending_ && completion_->GetCompletedValue() >= fenceValue_;
}
void MaskRefinerDirect3D::Impl::blockCpuUntilComplete()
{
	if (!pending_)
		throw std::logic_error("No mask work was submitted");
	if (isDone())
		return;
	TraceLoggingThreadActivity<provider, 0x1> wait;
	TraceLoggingWriteStart(wait, "MaskRefiner.CpuFenceWait");
	check(completion_->SetEventOnCompletion(fenceValue_, event_.get()), "Set mask completion event");
	for (;;) {
		auto result = WaitForSingleObject(event_.get(), 1000);
		check(device_->GetDeviceRemovedReason(), "Mask device removed while waiting");
		if (result == WAIT_OBJECT_0)
			break;
		if (result != WAIT_TIMEOUT)
			throw std::runtime_error("Mask completion wait failed");
	}
}
void MaskRefinerDirect3D::Impl::complete()
{
	if (!isDone())
		throw std::logic_error("Mask result is not complete");
	check(context_->Wait(completion11_.Get(), fenceValue_), "Order D3D11 mask reads");
	void *data = nullptr;
	D3D12_RANGE range{0, 4};
	check(statusReadback_->Map(0, &range, &data), "Map mask validation result");
	UINT status = 0;
	std::memcpy(&status, data, 4);
	D3D12_RANGE none{0, 0};
	statusReadback_->Unmap(0, &none);
	pending_ = false;
	if (status)
		throw std::runtime_error("Non-finite model probability");
}
MaskRefinerDirect3D::MaskRefinerDirect3D(ID3D12Device *d, ID3D12CommandQueue *q, ID3D11Device *g, std::uint32_t iw,
					 std::uint32_t ih, std::uint32_t ow, std::uint32_t oh)
	: impl_(std::make_unique<Impl>(d, q, g, iw, ih, ow, oh))
{
}
MaskRefinerDirect3D::~MaskRefinerDirect3D() = default;
void MaskRefinerDirect3D::runAsync(ID3D12Resource *input, const Settings &settings)
{
	impl_->runAsync(input, settings);
}
auto MaskRefinerDirect3D::isDone() const -> bool
{
	return impl_->isDone();
}
void MaskRefinerDirect3D::blockCpuUntilComplete()
{
	impl_->blockCpuUntilComplete();
}
void MaskRefinerDirect3D::complete()
{
	impl_->complete();
}
auto MaskRefinerDirect3D::outputTexture() const noexcept -> ID3D11Texture2D *
{
	return impl_->output11_.Get();
}
} // namespace BackgroundRemoval
