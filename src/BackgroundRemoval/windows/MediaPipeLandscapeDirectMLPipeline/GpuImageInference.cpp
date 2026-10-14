// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
// SPDX-License-Identifier: GPL-3.0-or-later
#include "GpuImageInference.hpp"
#include "DmlProgram/PerformanceTrace.hpp"
#include <d3d12.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <array>
#include <bit>
#include <cmath>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <tuple>
#include <optional>

namespace BackgroundRemoval::MediaPipeLandscapeDirectML {
namespace {
using Microsoft::WRL::ComPtr;
void check(HRESULT hr, const char *operation)
{
	if (FAILED(hr))
		throw std::runtime_error(std::string(operation) + " failed (HRESULT " + std::to_string(hr) + ")");
}
struct CloseHandleDeleter {
	void operator()(void *value) const noexcept
	{
		if (value)
			CloseHandle(value);
	}
};
using Handle = std::unique_ptr<void, CloseHandleDeleter>;
auto buffer(ID3D12Device *device, std::uint64_t bytes, D3D12_RESOURCE_STATES state,
	    D3D12_HEAP_TYPE type = D3D12_HEAP_TYPE_DEFAULT) -> ComPtr<ID3D12Resource>
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
	ComPtr<ID3D12Resource> result;
	check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr,
					      IID_PPV_ARGS(&result)),
	      "Create image inference buffer");
	return result;
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
void uavBarrier(ID3D12GraphicsCommandList *commands)
{
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	commands->ResourceBarrier(1, &barrier);
}
constexpr char source[] = R"(
cbuffer Settings : register(b0) { uint width; uint count; uint groups; float limit; uint force; }
Texture2D<float> image : register(t0);
StructuredBuffer<float> referenceImage : register(t1);
RWStructuredBuffer<float> currentImage : register(u0);
RWStructuredBuffer<float> partial : register(u1);
RWByteAddressBuffer decision : register(u2);
groupshared float sums[256];
[numthreads(256, 1, 1)]
void PackDifference(uint3 id : SV_DispatchThreadID, uint3 group : SV_GroupID, uint lane : SV_GroupIndex)
{
    float error = 0;
    if (id.x < count) {
        float value = image.Load(int3(id.x % width, id.x / width, 0));
        currentImage[id.x] = value;
        float difference = force ? 0 : value - referenceImage[id.x];
        error = isfinite(value) ? difference * difference : asfloat(0x7fc00000);
    }
    sums[lane] = error;
    GroupMemoryBarrierWithGroupSync();
    for (uint stride = 128; stride > 0; stride >>= 1) {
        if (lane < stride) sums[lane] += sums[lane + stride];
        GroupMemoryBarrierWithGroupSync();
    }
    if (lane == 0) partial[group.x] = sums[0];
}
[numthreads(256, 1, 1)]
void Decide(uint lane : SV_GroupIndex)
{
    float total = 0;
    for (uint i = lane; i < groups; i += 256) total += partial[i];
    sums[lane] = total;
    GroupMemoryBarrierWithGroupSync();
    for (uint stride = 128; stride > 0; stride >>= 1) {
        if (lane < stride) sums[lane] += sums[lane + stride];
        GroupMemoryBarrierWithGroupSync();
    }
    if (lane == 0) {
        bool valid = isfinite(sums[0]);
        // Identical images have infinite PSNR and never trigger a finite threshold.
        bool run = valid && (force || (sums[0] > 0 && sums[0] / count >= limit));
        decision.Store4(0, uint4(run ? 1 : 0, 0, valid ? 0 : 1, 0));
    }
}
)";
auto shader(const char *entry) -> ComPtr<ID3DBlob>
{
	ComPtr<ID3DBlob> code, errors;
	const auto hr = D3DCompile(source, sizeof(source) - 1, "GpuImageInference", nullptr, nullptr, entry, "cs_5_0",
				   D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
	if (FAILED(hr))
		throw std::runtime_error(errors ? std::string(static_cast<const char *>(errors->GetBufferPointer()),
							      errors->GetBufferSize())
						: "GPU gate shader compilation failed");
	return code;
}
} // namespace

struct GpuImageInference::Impl {
	BaselineDml::BaselineDmlProgramRuntime &runtime;
	std::uint32_t width, count, groups;
	ComPtr<ID3D12CommandAllocator> allocator;
	ComPtr<ID3D12GraphicsCommandList> commands;
	ComPtr<ID3D12Resource> current, reference, partial, decision, readback;
	ComPtr<ID3D12DescriptorHeap> descriptors;
	ComPtr<ID3D12RootSignature> root;
	ComPtr<ID3D12PipelineState> pack, decide;
	ComPtr<ID3D12Fence> completion;
	Handle event{CreateEventW(nullptr, FALSE, FALSE, nullptr)};
	std::uint64_t fenceValue = 0;
	D3D12_RESOURCE_STATES currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	D3D12_RESOURCE_STATES referenceState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	D3D12_RESOURCE_STATES decisionState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	bool referenceValid = false, canCommit = false;
	ComPtr<ID3D12QueryHeap> timestamps;
	ComPtr<ID3D12Resource> timestampReadback;
	UINT64 timestampFrequency = 0;
	using CommandKey = std::tuple<ID3D12Resource *, ID3D12Resource *, ID3D12Resource *, D3D12_RESOURCE_STATES,
				      D3D12_RESOURCE_STATES, D3D12_RESOURCE_STATES, std::uint32_t, bool, bool>;
	std::optional<CommandKey> recordedKey;
	GpuImageTimings timings;
	void mark(UINT index)
	{
		if (timestamps)
			commands->EndQuery(timestamps.Get(), D3D12_QUERY_TYPE_TIMESTAMP, index);
	}

	Impl(BaselineDml::BaselineDmlProgramRuntime &r, std::uint32_t w, std::uint32_t h)
		: runtime(r),
		  width(w),
		  count(0),
		  groups(0)
	{
		if (!w || !h || !event || static_cast<std::uint64_t>(w) * h > 65535ULL * 256 || r.inputCount() != 1 ||
		    r.inputInfo(0).type != BaselineDml::ProgramDefinition::DataType::Float32 ||
		    r.inputInfo(0).dataBytes != static_cast<std::uint64_t>(w) * h * sizeof(float) ||
		    r.outputInfo().type != BaselineDml::ProgramDefinition::DataType::Float32)
			throw std::invalid_argument(
				"GPU image inference requires one matching Float32 image input and output");
		count = w * h;
		groups = (count + 255) / 256;
		auto *device = r.device();
		current = buffer(device, count * sizeof(float), currentState);
		reference = buffer(device, count * sizeof(float), referenceState);
		partial = buffer(device, groups * sizeof(float), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		decision = buffer(device, 16, decisionState);
		readback = buffer(device, 16 + r.outputInfo().dataBytes, D3D12_RESOURCE_STATE_COPY_DEST,
				  D3D12_HEAP_TYPE_READBACK);
		check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)),
		      "Create GPU image allocator");
		check(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr,
						IID_PPV_ARGS(&commands)),
		      "Create GPU image command list");
		check(commands->Close(), "Close initial image command list");
		check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&completion)),
		      "Create image completion fence");
		D3D12_DESCRIPTOR_HEAP_DESC heap{};
		heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heap.NumDescriptors = 1;
		heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		check(device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&descriptors)),
		      "Create image texture descriptor");
		D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0};
		std::array<D3D12_ROOT_PARAMETER, 6> parameters{};
		parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		parameters[0].DescriptorTable = {1, &range};
		parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		parameters[1].Descriptor = {1, 0};
		for (UINT i = 2; i < 5; ++i) {
			parameters[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
			parameters[i].Descriptor = {i - 2, 0};
		}
		parameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		parameters[5].Constants = {0, 0, 5};
		D3D12_ROOT_SIGNATURE_DESC rootDesc{static_cast<UINT>(parameters.size()), parameters.data(), 0, nullptr,
						   D3D12_ROOT_SIGNATURE_FLAG_NONE};
		ComPtr<ID3DBlob> serialized, errors;
		check(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors),
		      "Serialize GPU gate root signature");
		check(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
						  IID_PPV_ARGS(&root)),
		      "Create GPU gate root signature");
		for (const auto entry : {"PackDifference", "Decide"}) {
			auto code = shader(entry);
			D3D12_COMPUTE_PIPELINE_STATE_DESC state{};
			state.pRootSignature = root.Get();
			state.CS = {code->GetBufferPointer(), code->GetBufferSize()};
			auto &pipeline = std::strcmp(entry, "Decide") == 0 ? decide : pack;
			check(device->CreateComputePipelineState(&state, IID_PPV_ARGS(&pipeline)),
			      "Create GPU gate pipeline");
		}
	}
	void wait()
	{
		if (completion->GetCompletedValue() < fenceValue) {
			check(completion->SetEventOnCompletion(fenceValue, event.get()), "Set image completion event");
			for (;;) {
				const auto result = WaitForSingleObject(event.get(), 1000);
				check(runtime.device()->GetDeviceRemovedReason(), "GPU image device removed");
				if (result == WAIT_OBJECT_0)
					break;
				if (result != WAIT_TIMEOUT)
					throw std::runtime_error("GPU image completion wait failed");
			}
		}
		check(runtime.device()->GetDeviceRemovedReason(), "GPU image submission");
	}
	~Impl()
	{
		try {
			wait();
		} catch (...) {
		}
	}
};

GpuImageInference::GpuImageInference(BaselineDml::BaselineDmlProgramRuntime &runtime, std::uint32_t width,
				     std::uint32_t height)
	: impl_(std::make_unique<Impl>(runtime, width, height))
{
}
GpuImageInference::~GpuImageInference() = default;

auto GpuImageInference::run(D3D12TextureReader &reader, gs_texture_t *input, float threshold, bool forceInference,
			    std::span<float> output) -> bool
{
	BaselineDml::PerformanceTrace::Scope frame("Pipeline.GpuGateAndInference");
	auto &s = *impl_;
	if (!std::isfinite(threshold) || output.size_bytes() != s.runtime.outputInfo().dataBytes)
		throw std::invalid_argument("GPU image output size or similarity threshold mismatch");
	s.wait();
	s.canCommit = false;
	using Clock = std::chrono::steady_clock;
	const bool profiling = bool(s.timestamps);
	auto start = profiling ? Clock::now() : Clock::time_point{};
	auto section = [&](double &value) {
		if (profiling) {
			const auto now = Clock::now();
			value = std::chrono::duration<double, std::milli>(now - start).count();
			start = now;
		}
	};
	s.timings = {};
	auto *texture = reader.capture(input);
	section(s.timings.captureCpuMs);
	// A completed command list can be submitted again while its resources and
	// initial states remain identical. Pixel contents and the GPU predicate vary
	// independently of the recorded commands.
	const Impl::CommandKey key{texture,
				   s.current.Get(),
				   s.reference.Get(),
				   s.currentState,
				   s.referenceState,
				   s.decisionState,
				   std::bit_cast<std::uint32_t>(threshold),
				   forceInference || !s.referenceValid,
				   profiling};
	auto *commands = s.commands.Get();
	if (!s.recordedKey || *s.recordedKey != key) {
		s.recordedKey.reset();
		ComPtr<ID3D12Device> textureDevice;
		check(texture->GetDevice(IID_PPV_ARGS(&textureDevice)), "Get shared texture device");
		if (textureDevice.Get() != s.runtime.device())
			throw std::invalid_argument("Texture reader and inference device mismatch");
		check(s.allocator->Reset(), "Reset GPU image allocator");
		check(s.commands->Reset(s.allocator.Get(), s.pack.Get()), "Reset GPU image commands");
		const auto desc = texture->GetDesc();
		if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || desc.Format != DXGI_FORMAT_R32_FLOAT ||
		    desc.Width != s.width || desc.Height != s.count / s.width)
			throw std::invalid_argument("GPU gate texture layout mismatch");
		D3D12_SHADER_RESOURCE_VIEW_DESC view{};
		view.Format = DXGI_FORMAT_R32_FLOAT;
		view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		view.Texture2D.MipLevels = 1;
		s.runtime.device()->CreateShaderResourceView(texture, &view,
							     s.descriptors->GetCPUDescriptorHandleForHeapStart());
		s.mark(0);
		transition(commands, texture, D3D12_RESOURCE_STATE_COMMON,
			   D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		transition(commands, s.current.Get(), s.currentState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		transition(commands, s.reference.Get(), s.referenceState,
			   D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		transition(commands, s.decision.Get(), s.decisionState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		ID3D12DescriptorHeap *heaps[]{s.descriptors.Get()};
		commands->SetDescriptorHeaps(1, heaps);
		commands->SetComputeRootSignature(s.root.Get());
		commands->SetComputeRootDescriptorTable(0, s.descriptors->GetGPUDescriptorHandleForHeapStart());
		commands->SetComputeRootShaderResourceView(1, s.reference->GetGPUVirtualAddress());
		commands->SetComputeRootUnorderedAccessView(2, s.current->GetGPUVirtualAddress());
		commands->SetComputeRootUnorderedAccessView(3, s.partial->GetGPUVirtualAddress());
		commands->SetComputeRootUnorderedAccessView(4, s.decision->GetGPUVirtualAddress());
		const float mseLimit = static_cast<float>(std::pow(10.0, -static_cast<double>(threshold) / 10.0));
		const std::uint32_t constants[]{s.width, s.count, s.groups, std::bit_cast<std::uint32_t>(mseLimit),
						forceInference || !s.referenceValid ? 1U : 0U};
		commands->SetComputeRoot32BitConstants(5, 5, constants, 0);
		commands->Dispatch(s.groups, 1, 1);
		uavBarrier(commands);
		s.mark(1);
		commands->SetPipelineState(s.decide.Get());
		commands->Dispatch(1, 1, 1);
		transition(commands, texture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			   D3D12_RESOURCE_STATE_COMMON);
		transition(commands, s.current.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			   D3D12_RESOURCE_STATE_COPY_SOURCE);
		transition(commands, s.decision.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			   D3D12_RESOURCE_STATE_PREDICATION);
		s.mark(2);
		const BaselineDml::GpuInput gpuInput{s.current.Get(), 0, s.runtime.inputInfo(0).dataBytes};
		s.runtime.recordGpuRun(commands, std::span(&gpuInput, 1), s.decision.Get(), s.readback.Get(), 16);
		s.mark(3);
		transition(commands, s.decision.Get(), D3D12_RESOURCE_STATE_PREDICATION,
			   D3D12_RESOURCE_STATE_COPY_SOURCE);
		commands->CopyBufferRegion(s.readback.Get(), 0, s.decision.Get(), 0, 16);
		s.mark(4);
		if (profiling)
			commands->ResolveQueryData(s.timestamps.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 5,
						   s.timestampReadback.Get(), 0);
		check(commands->Close(), "Close GPU image commands");
		s.recordedKey = key;
	}
	section(s.timings.recordCpuMs);
	ID3D12CommandList *lists[]{commands};
	s.runtime.queue()->ExecuteCommandLists(1, lists);
	const auto next = s.fenceValue + 1;
	check(s.runtime.queue()->Signal(s.completion.Get(), next), "Signal GPU image completion");
	s.fenceValue = next;
	reader.signalConsumed();
	s.currentState = D3D12_RESOURCE_STATE_COPY_SOURCE;
	s.referenceState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	s.decisionState = D3D12_RESOURCE_STATE_COPY_SOURCE;
	section(s.timings.submitCpuMs);
	{
		BaselineDml::PerformanceTrace::Scope waiting("Pipeline.GpuOutputWait");
		s.wait();
	}
	section(s.timings.waitCpuMs);
	BaselineDml::PerformanceTrace::Scope readback("Pipeline.MaskAndFlagReadback");
	const D3D12_RANGE range{0, static_cast<SIZE_T>(16 + output.size_bytes())};
	void *mapped = nullptr;
	check(s.readback->Map(0, &range, &mapped), "Map completed mask and execution flag");
	std::uint32_t header[4];
	std::memcpy(header, mapped, sizeof(header));
	if (header[0] && !header[2])
		std::memcpy(output.data(), static_cast<const std::byte *>(mapped) + 16, output.size_bytes());
	const D3D12_RANGE noWrite{0, 0};
	s.readback->Unmap(0, &noWrite);
	section(s.timings.readbackCpuMs);
	if (profiling) {
		const D3D12_RANGE timestampRange{0, 5 * sizeof(UINT64)};
		check(s.timestampReadback->Map(0, &timestampRange, &mapped), "Map GPU timestamps");
		UINT64 ticks[5];
		std::memcpy(ticks, mapped, sizeof(ticks));
		s.timestampReadback->Unmap(0, &noWrite);
		double *durations[]{&s.timings.packGpuMs, &s.timings.gateGpuMs, &s.timings.modelGpuMs,
				    &s.timings.packetGpuMs};
		for (UINT i = 0; i < 4; ++i)
			*durations[i] = double(ticks[i + 1] - ticks[i]) * 1000 / s.timestampFrequency;
	}
	if (header[2])
		throw std::runtime_error("Non-finite GPU model input or image difference");
	s.canCommit = header[0] != 0;
	return s.canCommit;
}

void GpuImageInference::enableProfiling()
{
	auto &s = *impl_;
	s.wait();
	if (s.timestamps)
		return;
	D3D12_QUERY_HEAP_DESC desc{D3D12_QUERY_HEAP_TYPE_TIMESTAMP, 5, 0};
	check(s.runtime.queue()->GetTimestampFrequency(&s.timestampFrequency), "Get GPU timestamp frequency");
	check(s.runtime.device()->CreateQueryHeap(&desc, IID_PPV_ARGS(&s.timestamps)), "Create GPU timestamp heap");
	s.timestampReadback = buffer(s.runtime.device(), 5 * sizeof(UINT64), D3D12_RESOURCE_STATE_COPY_DEST,
				     D3D12_HEAP_TYPE_READBACK);
}
auto GpuImageInference::lastTimings() const noexcept -> GpuImageTimings
{
	return impl_->timings;
}

void GpuImageInference::commitReference()
{
	auto &s = *impl_;
	if (!s.canCommit)
		throw std::logic_error("Cannot commit a skipped or failed inference");
	std::swap(s.current, s.reference);
	std::swap(s.currentState, s.referenceState);
	s.referenceValid = true;
	s.canCommit = false;
}
} // namespace BackgroundRemoval::MediaPipeLandscapeDirectML
