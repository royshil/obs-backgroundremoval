// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef NOMINMAX
#define NOMINMAX
#include <winerror.h>
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "DmlProgramCompiler.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <d3d12.h>
#include <d3dcompiler.h>

#ifndef DML_TARGET_VERSION
#define DML_TARGET_VERSION 0x1000
#elif DML_TARGET_VERSION != 0x1000
#error "DML_TARGET_VERSION is not 0x1000"
#endif
#include <DirectML.h>

namespace {

struct IUnknownReleaser final {
	void operator()(IUnknown *object) const noexcept
	{
		if (object)
			object->Release();
	}
};

constexpr auto bufferResourceDesc(std::uint64_t size, D3D12_RESOURCE_FLAGS flags) noexcept -> D3D12_RESOURCE_DESC;
constexpr auto bufferTensorDesc(const BaselineDml::TensorSpec &spec) noexcept -> DML_BUFFER_TENSOR_DESC;
auto makeDmlPersistentResource(ID3D12Device *device, IDMLCompiledOperator *operation)
	-> std::unique_ptr<ID3D12Resource, IUnknownReleaser>;

auto makeDmlPersistentResource(ID3D12Device *device, IDMLCompiledOperator *operation)
	-> std::unique_ptr<ID3D12Resource, IUnknownReleaser>
{
	const auto size = operation->GetBindingProperties().PersistentResourceSize;
	if (!size)
		return {nullptr, {}};

	D3D12_HEAP_PROPERTIES heap{};
	heap.Type = D3D12_HEAP_TYPE_DEFAULT;
	const auto description = bufferResourceDesc(size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	ID3D12Resource *resource = nullptr;
	if (const HRESULT result = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
								   D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
								   IID_PPV_ARGS(&resource));
	    FAILED(result)) {
		if (resource != nullptr)
			resource->Release();
		throw BaselineDml::DmlProgramCompilerError("Create DML persistent buffer failed", result);
	}
	return {resource, {}};
}

} // namespace

namespace BaselineDml {

namespace {

void releaseShaderCom(IUnknown *object) noexcept
{
	object->Release();
}

struct CustomComputeShader final {
	const char *name;
	std::string_view source;
	std::array<std::uint32_t, 4> rootUavRegisters{0, 1, 2, 3};

	void operator()(ID3D12Device *device, std::unique_ptr<ID3D12RootSignature, IUnknownReleaser> &rootSignature,
			std::unique_ptr<ID3D12PipelineState, IUnknownReleaser> &pipeline) const
	{
		std::unique_ptr<ID3DBlob, void (*)(IUnknown *)> bytecode{nullptr, releaseShaderCom};
		std::unique_ptr<ID3DBlob, void (*)(IUnknown *)> errors{nullptr, releaseShaderCom};
		ID3DBlob *bytecodePointer = nullptr;
		ID3DBlob *compileErrorsPointer = nullptr;
		const auto compileResult = D3DCompile(source.data(), source.size(), name, nullptr, nullptr, "main",
						      "cs_5_0",
						      D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_IEEE_STRICTNESS, 0,
						      &bytecodePointer, &compileErrorsPointer);
		bytecode.reset(bytecodePointer);
		errors.reset(compileErrorsPointer);
		if (FAILED(compileResult))
			throw std::runtime_error(std::string("Compile ") + name + " failed (HRESULT " +
						 std::to_string(compileResult) + ")");

		std::array<D3D12_ROOT_PARAMETER, 4> parameters{};
		for (std::size_t index = 0; index < parameters.size(); ++index) {
			parameters[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
			parameters[index].Descriptor.ShaderRegister = rootUavRegisters[index];
		}
		D3D12_ROOT_SIGNATURE_DESC root{};
		root.NumParameters = static_cast<UINT>(parameters.size());
		root.pParameters = parameters.data();
		std::unique_ptr<ID3DBlob, void (*)(IUnknown *)> serialized{nullptr, releaseShaderCom};
		ID3DBlob *serializedPointer = nullptr;
		ID3DBlob *serializationErrorsPointer = nullptr;
		const auto serializationResult = D3D12SerializeRootSignature(
			&root, D3D_ROOT_SIGNATURE_VERSION_1, &serializedPointer, &serializationErrorsPointer);
		serialized.reset(serializedPointer);
		errors.reset(serializationErrorsPointer);
		if (FAILED(serializationResult))
			throw std::runtime_error(std::string("Serialize shader root signature failed (HRESULT ") +
						 std::to_string(serializationResult) + ")");

		ID3D12RootSignature *rootSignaturePointer = nullptr;
		const auto rootResult = device->CreateRootSignature(0, serialized->GetBufferPointer(),
								    serialized->GetBufferSize(),
								    IID_PPV_ARGS(&rootSignaturePointer));
		rootSignature.reset(rootSignaturePointer);
		if (FAILED(rootResult))
			throw std::runtime_error(std::string("Create shader root signature failed (HRESULT ") +
						 std::to_string(rootResult) + ")");

		D3D12_COMPUTE_PIPELINE_STATE_DESC description{};
		description.pRootSignature = rootSignature.get();
		description.CS = {bytecode->GetBufferPointer(), bytecode->GetBufferSize()};
		ID3D12PipelineState *pipelinePointer = nullptr;
		const auto pipelineResult =
			device->CreateComputePipelineState(&description, IID_PPV_ARGS(&pipelinePointer));
		pipeline.reset(pipelinePointer);
		if (FAILED(pipelineResult))
			throw std::runtime_error(std::string("Create shader pipeline failed (HRESULT ") +
						 std::to_string(pipelineResult) + ")");
	}
};

const CustomComputeShader kDepthwiseConvShader{
	.name = "MPLD10DepthwiseConv2dRelu",
	.source = R"HLSL(
static const uint kInputWidth = 128;
static const uint kInputHeight = 72;
static const uint kChannelCount = 16;
static const uint kOutputWidth = 128;
static const uint kOutputHeight = 72;
static const int kPaddingX = 1;
static const int kPaddingY = 1;
static const uint kRowsPerThread = 8;
static const uint kThreadsPerGroup = 64;

RWByteAddressBuffer inputTensor : register(u0);
RWByteAddressBuffer filterWeights : register(u1);
RWByteAddressBuffer channelBias : register(u2);
RWByteAddressBuffer outputTensor : register(u3);

// Read three horizontally adjacent samples, keeping padded reads inside the input buffer.
float3 loadInputRow(uint channel, uint outputX, int inputY)
{
    float3 row;
    [unroll]
    for (uint kernelX = 0; kernelX < 3; ++kernelX) {
        int inputX = int(outputX) + int(kernelX) - kPaddingX;
        int clampedX = clamp(inputX, 0, int(kInputWidth) - 1);
        int clampedY = clamp(inputY, 0, int(kInputHeight) - 1);
        uint inputIndex = (channel * kInputHeight + uint(clampedY)) * kInputWidth + uint(clampedX);
        float inputValue = asfloat(inputTensor.Load(4 * inputIndex));
        bool insideInput = inputX >= 0 && inputX < int(kInputWidth) &&
                           inputY >= 0 && inputY < int(kInputHeight);
        row[kernelX] = insideInput ? inputValue : 0.0f;
    }
    return row;
}

[numthreads(kThreadsPerGroup, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    // Each thread handles one vertical strip. Adjacent threads cover adjacent x coordinates.
    const uint stripsPerChannel = (kOutputHeight + kRowsPerThread - 1) / kRowsPerThread;
    uint threadIndex = dispatchThreadId.x;
    if (threadIndex >= kChannelCount * stripsPerChannel * kOutputWidth)
        return;

    uint channel = threadIndex / (stripsPerChannel * kOutputWidth);
    uint outputX = threadIndex % kOutputWidth;
    uint firstOutputY = ((threadIndex / kOutputWidth) % stripsPerChannel) * kRowsPerThread;

    float3 filterTop;
    float3 filterMiddle;
    float3 filterBottom;
    [unroll]
    for (uint kernelX = 0; kernelX < 3; ++kernelX) {
        filterTop[kernelX] = asfloat(filterWeights.Load(4 * (channel * 9 + kernelX)));
        filterMiddle[kernelX] = asfloat(filterWeights.Load(4 * (channel * 9 + 3 + kernelX)));
        filterBottom[kernelX] = asfloat(filterWeights.Load(4 * (channel * 9 + 6 + kernelX)));
    }
    float bias = asfloat(channelBias.Load(4 * channel));

    // Retain two rows between output samples; only the next bottom row needs loading.
    float3 rowTop = loadInputRow(channel, outputX, int(firstOutputY) - kPaddingY);
    float3 rowMiddle = loadInputRow(channel, outputX, int(firstOutputY) + 1 - kPaddingY);
    [unroll]
    for (uint rowOffset = 0; rowOffset < kRowsPerThread; ++rowOffset) {
        float3 rowBottom = loadInputRow(channel, outputX, int(firstOutputY + rowOffset) + 2 - kPaddingY);
        precise float sum = 0.0f;
        [unroll]
        for (uint kernelX = 0; kernelX < 3; ++kernelX)
            sum = sum + rowTop[kernelX] * filterTop[kernelX];
        [unroll]
        for (uint kernelX = 0; kernelX < 3; ++kernelX)
            sum = sum + rowMiddle[kernelX] * filterMiddle[kernelX];
        [unroll]
        for (uint kernelX = 0; kernelX < 3; ++kernelX)
            sum = sum + rowBottom[kernelX] * filterBottom[kernelX];

        uint outputY = firstOutputY + rowOffset;
        if (outputY < kOutputHeight) {
            uint outputIndex = (channel * kOutputHeight + outputY) * kOutputWidth + outputX;
            outputTensor.Store(4 * outputIndex, asuint(max(sum + bias, 0.0f)));
        }
        rowTop = rowMiddle;
        rowMiddle = rowBottom;
    }
}
)HLSL",
};

const CustomComputeShader kMaskShader{
	.name = "MPLD10BinarizeFloat32",
	.source = R"HLSL(
static const uint kMaskWidth = 256;
static const uint kMaskHeight = 144;
static const uint kThreadsPerGroup = 64;

RWStructuredBuffer<float> probabilities : register(u0);
RWStructuredBuffer<float> threshold : register(u1);
RWStructuredBuffer<float> mask : register(u3);

[numthreads(kThreadsPerGroup, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint pixelIndex = dispatchThreadId.x;
    if (pixelIndex >= kMaskWidth * kMaskHeight)
        return;

    mask[pixelIndex] = probabilities[pixelIndex] < threshold[0] ? 1.0f : 0.0f;
}
)HLSL",
};

} // namespace

struct DmlProgramCompiler::Impl final {
	template<typename T> using NodeVector = std::vector<T>;
	static void releaseIUnknown(IUnknown *object) noexcept { IUnknownReleaser{}(object); }

	Impl(IDMLDevice *device, const DmlProgramDefinition &definition);

	void describeInitializationInputs(std::vector<DML_BINDING_DESC> &inputs);
	void describeInitializationOutputs(std::vector<DML_BINDING_DESC> &outputs);
	auto signalCompletion() const noexcept -> HRESULT;
	void waitForCompletion() const;
	auto makeNodeTensorResource(std::int64_t nodeIndex, D3D12_RESOURCE_STATES initialState)
		-> std::shared_ptr<ID3D12Resource>;

	// Borrowed inputs and the parent device owner. The device is declared before its dependents so it is
	// released after them.
	const DmlProgramDefinition definition_;
	IDMLDevice *const dmlDevice_;
	std::unique_ptr<ID3D12Device, IUnknownReleaser> d3dDevice_;
	std::int64_t nodeIndex_ = -1;

	// Node-indexed owners transferred to DmlProgram by build().
	std::shared_ptr<ID3D12Resource> weight_;
	NodeVector<std::shared_ptr<ID3D12Resource>> tensorResources_;
	NodeVector<std::unique_ptr<ID3D12Resource, IUnknownReleaser>> dmlPersistentResources_;
	NodeVector<std::unique_ptr<IDMLCompiledOperator, IUnknownReleaser>> operations_;
	NodeVector<std::unique_ptr<ID3D12RootSignature, IUnknownReleaser>> rootSignatures_;
	NodeVector<std::unique_ptr<ID3D12PipelineState, IUnknownReleaser>> pipelines_;
	// Non-owning pointers in initializer order; operations_ owns the operators.
	std::vector<IDMLCompiledOperator *> operatorsToReset_;

	// Node-indexed storage for descriptors passed to the batched initializer.
	NodeVector<std::vector<DML_BUFFER_BINDING>> initializationInputBuffers_;
	NodeVector<DML_BUFFER_ARRAY_BINDING> initializationInputArrays_;
	NodeVector<DML_BUFFER_BINDING> initializationPersistentBuffers_;

	// DML initialization dependencies. The binding table is declared last so it is released first.
	std::unique_ptr<IDMLCommandRecorder, IUnknownReleaser> recorder_;
	std::unique_ptr<ID3D12Resource, IUnknownReleaser> weightUpload_;
	std::unique_ptr<ID3D12Resource, IUnknownReleaser> initializationTemporary_;
	std::unique_ptr<IDMLOperatorInitializer, IUnknownReleaser> initializer_;
	std::unique_ptr<ID3D12DescriptorHeap, IUnknownReleaser> initializationHeap_;
	std::unique_ptr<IDMLBindingTable, IUnknownReleaser> initializationTable_;

	// Submission context. Declared last so command recording objects are released before their resources.
	std::unique_ptr<ID3D12CommandQueue, IUnknownReleaser> queue_;
	std::unique_ptr<ID3D12Fence, IUnknownReleaser> fence_;
	std::unique_ptr<ID3D12CommandAllocator, IUnknownReleaser> allocator_;
	std::unique_ptr<ID3D12GraphicsCommandList, IUnknownReleaser> commandList_;
};

auto DmlProgramCompiler::makeImpl(IDMLDevice *device, const DmlProgramDefinition &definition) -> std::unique_ptr<Impl>
{
	if (device == nullptr) {
		throw std::invalid_argument("device is null");
	} else if (definition.nodes.empty()) {
		throw std::invalid_argument("node list must not be empty");
	} else if (definition.weights.empty() || definition.params.weightBytes == 0) {
		throw std::invalid_argument("weight data must not be empty");
	} else if (definition.weights.size_bytes() != definition.params.sourceWeightBytes) {
		throw std::invalid_argument("source weight byte size differs from the prepared model");
	} else if (definition.params.tensors.size() != definition.nodes.size()) {
		throw std::invalid_argument("tensor count differs from the number of nodes");
	} else if (definition.params.steps.size() != definition.nodes.size()) {
		throw std::invalid_argument("step count differs from the number of nodes");
	}

	return std::make_unique<DmlProgramCompiler::Impl>(device, definition);
}

DmlProgramCompiler::DmlProgramCompiler(IDMLDevice *device, const DmlProgramDefinition &definition)
	: impl_(makeImpl(device, definition))
{
}

DmlProgramCompiler::~DmlProgramCompiler() noexcept = default;

namespace {

auto getParentDevice(IDMLDevice *device) -> std::unique_ptr<ID3D12Device, IUnknownReleaser>
{
	if (device == nullptr) {
		throw std::invalid_argument("device is null");
	}

	ID3D12Device *p = nullptr;

	if (const HRESULT hr = device->GetParentDevice(IID_PPV_ARGS(&p)); FAILED(hr)) {
		if (p != nullptr) {
			p->Release();
		}
		throw DmlProgramCompiler::WinError("Failed to get parent device", hr);
	}

	return std::unique_ptr<ID3D12Device, IUnknownReleaser>{p};
}

auto makeCommandQueue(ID3D12Device *device) -> std::unique_ptr<ID3D12CommandQueue, IUnknownReleaser>
{
	if (device == nullptr) {
		throw std::invalid_argument("device is null");
	}

	ID3D12CommandQueue *p = nullptr;

	D3D12_COMMAND_QUEUE_DESC description{
		.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
	};
	if (const HRESULT hr = device->CreateCommandQueue(&description, IID_PPV_ARGS(&p)); FAILED(hr)) {
		if (p != nullptr) {
			p->Release();
		}
		throw DmlProgramCompiler::WinError("Failed to create command queue", hr);
	}

	return std::unique_ptr<ID3D12CommandQueue, IUnknownReleaser>{p};
}

auto makeCommandAllocator(ID3D12Device *device) -> std::unique_ptr<ID3D12CommandAllocator, IUnknownReleaser>
{
	if (device == nullptr) {
		throw std::invalid_argument("device is null");
	}

	ID3D12CommandAllocator *p = nullptr;

	if (const HRESULT hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&p));
	    FAILED(hr)) {
		if (p != nullptr) {
			p->Release();
		}
		throw DmlProgramCompiler::WinError("Failed to create command allocator", hr);
	}

	return std::unique_ptr<ID3D12CommandAllocator, IUnknownReleaser>{p};
}

auto makeCommandList(ID3D12Device *device, ID3D12CommandAllocator *allocator)
	-> std::unique_ptr<ID3D12GraphicsCommandList, IUnknownReleaser>
{
	if (device == nullptr) {
		throw std::invalid_argument("device is null");
	} else if (allocator == nullptr) {
		throw std::invalid_argument("allocator is null");
	}

	ID3D12GraphicsCommandList *p = nullptr;

	if (const HRESULT hr =
		    device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, nullptr, IID_PPV_ARGS(&p));
	    FAILED(hr)) {
		if (p != nullptr) {
			p->Release();
		}
		throw DmlProgramCompiler::WinError("Failed to create command list", hr);
	}

	return std::unique_ptr<ID3D12GraphicsCommandList, IUnknownReleaser>{p};
}

auto makeFence(ID3D12Device *device) -> std::unique_ptr<ID3D12Fence, IUnknownReleaser>
{
	if (device == nullptr) {
		throw std::invalid_argument("device is null");
	}

	ID3D12Fence *p = nullptr;

	if (const HRESULT hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&p)); FAILED(hr)) {
		if (p != nullptr) {
			p->Release();
		}
		throw DmlProgramCompiler::WinError("Failed to create fence", hr);
	}

	return std::unique_ptr<ID3D12Fence, IUnknownReleaser>{p};
}

auto makeCommandRecorder(IDMLDevice *device) -> std::unique_ptr<IDMLCommandRecorder, IUnknownReleaser>
{
	if (device == nullptr) {
		throw std::invalid_argument("device is null");
	}

	IDMLCommandRecorder *p = nullptr;

	if (const HRESULT hr = device->CreateCommandRecorder(IID_PPV_ARGS(&p)); FAILED(hr)) {
		if (p != nullptr) {
			p->Release();
		}
		throw DmlProgramCompiler::WinError("Failed to create command recorder", hr);
	}

	return std::unique_ptr<IDMLCommandRecorder, IUnknownReleaser>{p};
}

auto makeOperatorInitializer(IDMLDevice *device) -> std::unique_ptr<IDMLOperatorInitializer, IUnknownReleaser>
{
	if (device == nullptr) {
		throw std::invalid_argument("device is null");
	}

	IDMLOperatorInitializer *p = nullptr;

	if (const HRESULT hr = device->CreateOperatorInitializer(0, nullptr, IID_PPV_ARGS(&p)); FAILED(hr)) {
		if (p != nullptr) {
			p->Release();
		}
		throw DmlProgramCompiler::WinError("Failed to create operator initializer", hr);
	}

	return std::unique_ptr<IDMLOperatorInitializer, IUnknownReleaser>{p};
}

auto makeWeightUpload(ID3D12Device *device, std::uint64_t size) -> std::unique_ptr<ID3D12Resource, IUnknownReleaser>
{
	if (device == nullptr) {
		throw std::invalid_argument("device is null");
	}

	if (size < 4) {
		size = 4;
	}

	ID3D12Resource *p = nullptr;

	D3D12_HEAP_PROPERTIES heap{
		.Type = D3D12_HEAP_TYPE_UPLOAD,
	};
	const D3D12_RESOURCE_DESC description{
		.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
		.Width = size,
		.Height = 1,
		.DepthOrArraySize = 1,
		.MipLevels = 1,
		.SampleDesc = {1, 0},
		.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
		.Flags = D3D12_RESOURCE_FLAG_NONE,
	};
	if (const HRESULT hr = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
							       D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
							       IID_PPV_ARGS(&p));
	    FAILED(hr)) {
		if (p != nullptr) {
			p->Release();
		}
		throw DmlProgramCompiler::WinError("Failed to create weight upload buffer", hr);
	}

	return std::unique_ptr<ID3D12Resource, IUnknownReleaser>{p};
}

auto makeWeight(ID3D12Device *device, std::uint64_t size) -> std::unique_ptr<ID3D12Resource, IUnknownReleaser>
{
	if (device == nullptr) {
		throw std::invalid_argument("device is null");
	}

	if (size < 4) {
		size = 4;
	}

	ID3D12Resource *p = nullptr;

	D3D12_HEAP_PROPERTIES heap{
		.Type = D3D12_HEAP_TYPE_DEFAULT,
	};
	const D3D12_RESOURCE_DESC description{
		.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
		.Width = size,
		.Height = 1,
		.DepthOrArraySize = 1,
		.MipLevels = 1,
		.SampleDesc = {1, 0},
		.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
	};
	if (const HRESULT hr = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
							       D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
							       IID_PPV_ARGS(&p));
	    FAILED(hr)) {
		if (p != nullptr) {
			p->Release();
		}
		throw DmlProgramCompiler::WinError("Failed to create GPU weight buffer", hr);
	}

	return std::unique_ptr<ID3D12Resource, IUnknownReleaser>{p};
}

auto makeTensor(ID3D12Device *device, std::uint64_t size, D3D12_RESOURCE_STATES initialState)
	-> std::unique_ptr<ID3D12Resource, IUnknownReleaser>
{
	if (device == nullptr) {
		throw std::invalid_argument("device is null");
	}

	if (size < 4) {
		size = 4;
	}

	ID3D12Resource *p = nullptr;

	D3D12_HEAP_PROPERTIES heap{
		.Type = D3D12_HEAP_TYPE_DEFAULT,
	};
	const D3D12_RESOURCE_DESC description{
		.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
		.Width = size,
		.Height = 1,
		.DepthOrArraySize = 1,
		.MipLevels = 1,
		.SampleDesc = {1, 0},
		.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
	};
	if (const HRESULT hr = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description, initialState,
							       nullptr, IID_PPV_ARGS(&p));
	    FAILED(hr)) {
		if (p != nullptr) {
			p->Release();
		}
		throw DmlProgramCompiler::WinError("Failed to create tensor buffer", hr);
	}

	return std::unique_ptr<ID3D12Resource, IUnknownReleaser>{p};
}

auto makeInitializationHeap(ID3D12Device *device, UINT descriptorCount)
	-> std::unique_ptr<ID3D12DescriptorHeap, IUnknownReleaser>
{
	if (device == nullptr) {
		throw std::invalid_argument("device is null");
	}

	ID3D12DescriptorHeap *p = nullptr;

	D3D12_DESCRIPTOR_HEAP_DESC description{
		.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		.NumDescriptors = descriptorCount,
		.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
	};
	if (const HRESULT hr = device->CreateDescriptorHeap(&description, IID_PPV_ARGS(&p)); FAILED(hr)) {
		if (p != nullptr) {
			p->Release();
		}
		throw DmlProgramCompiler::WinError("Failed to create initialization heap", hr);
	}

	return std::unique_ptr<ID3D12DescriptorHeap, IUnknownReleaser>{p};
}

auto makeInitializationTable(IDMLDevice *device, const DML_BINDING_TABLE_DESC &description)
	-> std::unique_ptr<IDMLBindingTable, IUnknownReleaser>
{
	if (device == nullptr) {
		throw std::invalid_argument("device is null");
	}

	IDMLBindingTable *p = nullptr;

	if (const HRESULT hr = device->CreateBindingTable(&description, IID_PPV_ARGS(&p)); FAILED(hr)) {
		if (p != nullptr) {
			p->Release();
		}
		throw DmlProgramCompiler::WinError("Failed to create initialization binding table", hr);
	}

	return std::unique_ptr<IDMLBindingTable, IUnknownReleaser>{p};
}

auto makeInitializationTemporary(ID3D12Device *device, std::uint64_t size)
	-> std::unique_ptr<ID3D12Resource, IUnknownReleaser>
{
	if (device == nullptr) {
		throw std::invalid_argument("device is null");
	}

	if (size < 4) {
		size = 4;
	}

	ID3D12Resource *p = nullptr;

	D3D12_HEAP_PROPERTIES heap{
		.Type = D3D12_HEAP_TYPE_DEFAULT,
	};
	const D3D12_RESOURCE_DESC description{
		.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
		.Width = size,
		.Height = 1,
		.DepthOrArraySize = 1,
		.MipLevels = 1,
		.SampleDesc = {1, 0},
		.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
	};

	if (const HRESULT hr = device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description,
							       D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
							       IID_PPV_ARGS(&p));
	    FAILED(hr)) {
		if (p != nullptr) {
			p->Release();
		}
		throw DmlProgramCompiler::WinError("Failed to create DML initialization temporary buffer", hr);
	}

	return std::unique_ptr<ID3D12Resource, IUnknownReleaser>{p};
}

auto makeInitializationInputBuffers(const DmlProgramDefinition &definition)
	-> std::vector<std::vector<DML_BUFFER_BINDING>>
{
	std::vector<std::vector<DML_BUFFER_BINDING>> buffers;
	buffers.reserve(definition.params.steps.size());
	for (const auto &step : definition.params.steps) {
		buffers.emplace_back(step.inputCount);
	}
	return buffers;
}

} // namespace

auto DmlProgramCompiler::Impl::makeNodeTensorResource(std::int64_t nodeIndex, D3D12_RESOURCE_STATES initialState)
	-> std::shared_ptr<ID3D12Resource>
{
	auto resource = makeTensor(d3dDevice_.get(), tensorSize(definition_.params.tensors[nodeIndex]), initialState);
	return std::shared_ptr<ID3D12Resource>{std::move(resource)};
}

DmlProgramCompiler::Impl::Impl(IDMLDevice *device, const DmlProgramDefinition &definition)
	: definition_(definition),
	  dmlDevice_(device),
	  d3dDevice_(getParentDevice(device)),
	  weight_(makeWeight(d3dDevice_.get(), definition_.params.weightBytes)),
	  tensorResources_(definition_.nodes.size()),
	  dmlPersistentResources_(definition_.nodes.size()),
	  operations_(definition_.nodes.size()),
	  rootSignatures_(definition_.nodes.size()),
	  pipelines_(definition_.nodes.size()),
	  initializationInputBuffers_(makeInitializationInputBuffers(definition_)),
	  initializationInputArrays_(definition_.params.steps.size()),
	  initializationPersistentBuffers_(definition_.params.steps.size()),
	  recorder_(makeCommandRecorder(device)),
	  weightUpload_(makeWeightUpload(d3dDevice_.get(), definition_.params.weightBytes)),
	  initializer_(makeOperatorInitializer(device)),
	  queue_(makeCommandQueue(d3dDevice_.get())),
	  fence_(makeFence(d3dDevice_.get())),
	  allocator_(makeCommandAllocator(d3dDevice_.get())),
	  commandList_(makeCommandList(d3dDevice_.get(), allocator_.get()))
{
	operatorsToReset_.reserve(definition_.nodes.size());
	void *mapped = nullptr;
	if (const HRESULT hr = weightUpload_->Map(0, nullptr, &mapped); FAILED(hr)) {
		throw DmlProgramCompiler::WinError("Failed to map weight upload buffer", hr);
	}

	const auto weightBytes = std::as_bytes(definition_.weights);
	std::memcpy(mapped, weightBytes.data(), weightBytes.size());
	weightUpload_->Unmap(0, nullptr);
}

void DmlProgramCompiler::beginProgram()
{
	// Copy the prepared weight bytes into the GPU buffer before DirectML binds it as a UAV.

	impl_->commandList_->CopyBufferRegion(impl_->weight_.get(), 0, impl_->weightUpload_.get(), 0,
					      impl_->definition_.params.weightBytes);

	const D3D12_RESOURCE_BARRIER weightBarrier{
		.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
		.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
		.Transition =
			{
				.pResource = impl_->weight_.get(),
				.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
				.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
				.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			},
	};

	impl_->commandList_->ResourceBarrier(1, &weightBarrier);
}

void DmlProgramCompiler::beginNode(std::int64_t nodeIndex)
{
	if (nodeIndex < 0 || static_cast<std::size_t>(nodeIndex) >= impl_->definition_.nodes.size()) {
		throw std::logic_error("node index is out of range");
	}

	impl_->nodeIndex_ = nodeIndex;
}

void DmlProgramCompiler::endNode(std::int64_t nodeIndex)
{
	if (impl_->nodeIndex_ != nodeIndex) {
		throw std::logic_error("node index does not match the active node");
	}

	impl_->nodeIndex_ = -1;
}

void DmlProgramCompiler::endProgram()
{
	if (impl_->operatorsToReset_.empty()) {
		throw std::logic_error("No DirectML operators to initialize");
	}

	if (impl_->operatorsToReset_.size() > std::numeric_limits<UINT>::max()) {
		throw std::invalid_argument("Too many operators for initialization");
	}
}

void DmlProgramCompiler::prepareInitialization()
{
	// Configure the initializer for the compiled operators and create its binding table.
	if (const HRESULT result = impl_->initializer_->Reset(static_cast<UINT>(impl_->operatorsToReset_.size()),
							      impl_->operatorsToReset_.data());
	    FAILED(result)) {
		throw DmlProgramCompilerError("Set initializer operators failed", result);
	}

	const auto properties = impl_->initializer_->GetBindingProperties();
	const auto descriptorCount = std::max<UINT>(1, properties.RequiredDescriptorCount);
	impl_->initializationHeap_ = makeInitializationHeap(impl_->d3dDevice_.get(), descriptorCount);
	const DML_BINDING_TABLE_DESC description{
		.Dispatchable = impl_->initializer_.get(),
		.CPUDescriptorHandle = impl_->initializationHeap_->GetCPUDescriptorHandleForHeapStart(),
		.GPUDescriptorHandle = impl_->initializationHeap_->GetGPUDescriptorHandleForHeapStart(),
		.SizeInDescriptors = descriptorCount,
	};
	impl_->initializationTable_ = makeInitializationTable(impl_->dmlDevice_, description);

	// Bind weights as inputs and the operators' persistent resources as outputs.
	{
		std::vector<DML_BINDING_DESC> inputs;
		inputs.reserve(impl_->operatorsToReset_.size());
		impl_->describeInitializationInputs(inputs);
		impl_->initializationTable_->BindInputs(static_cast<UINT>(inputs.size()), inputs.data());
	}
	{
		std::vector<DML_BINDING_DESC> outputs;
		outputs.reserve(impl_->operatorsToReset_.size());
		impl_->describeInitializationOutputs(outputs);
		impl_->initializationTable_->BindOutputs(static_cast<UINT>(outputs.size()), outputs.data());
	}
	if (properties.TemporaryResourceSize) {
		impl_->initializationTemporary_ =
			makeInitializationTemporary(impl_->d3dDevice_.get(), properties.TemporaryResourceSize);
		const DML_BUFFER_BINDING buffer{impl_->initializationTemporary_.get(), 0,
						properties.TemporaryResourceSize};
		const DML_BINDING_DESC binding{DML_BINDING_TYPE_BUFFER, &buffer};
		impl_->initializationTable_->BindTemporaryResource(&binding);
	}
	if (const HRESULT result = impl_->dmlDevice_->GetDeviceRemovedReason(); FAILED(result))
		throw DmlProgramCompilerError("Bind batched initializer resources failed", result);
}

void DmlProgramCompiler::recordInitialization()
{
	// Record initialization after the weight upload and order subsequent UAV accesses.
	ID3D12DescriptorHeap *heaps[]{impl_->initializationHeap_.get()};
	impl_->commandList_->SetDescriptorHeaps(1, heaps);
	impl_->recorder_->RecordDispatch(impl_->commandList_.get(), impl_->initializer_.get(),
					 impl_->initializationTable_.get());
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	impl_->commandList_->ResourceBarrier(1, &barrier);
}

void DmlProgramCompiler::submitAndWait()
{
	// Submit the upload and initialization, then retain their resources until completion.
	if (const HRESULT result = impl_->commandList_->Close(); FAILED(result))
		throw DmlProgramCompilerError("Close compiler commands failed", result);
	ID3D12CommandList *lists[]{impl_->commandList_.get()};
	impl_->queue_->ExecuteCommandLists(1, lists);
	const HRESULT signal = impl_->signalCompletion();
	impl_->waitForCompletion();
	if (FAILED(signal))
		throw DmlProgramCompilerError("Signal compiler completion failed", signal);
	if (const HRESULT result = impl_->dmlDevice_->GetDeviceRemovedReason(); FAILED(result))
		throw DmlProgramCompilerError("Compiler DirectML completion failed", result);
}

auto DmlProgramCompiler::build() -> std::unique_ptr<DmlProgram>
{
	auto program = std::make_unique<DmlProgram>();
	program->tensorResources = std::move(impl_->tensorResources_);
	impl_->weight_.reset();
	program->dmlPersistentResources.reserve(impl_->dmlPersistentResources_.size());
	for (auto &resource : impl_->dmlPersistentResources_)
		program->dmlPersistentResources.emplace_back(resource.release(), Impl::releaseIUnknown);
	program->operations.reserve(impl_->operations_.size());
	for (auto &operation : impl_->operations_)
		program->operations.emplace_back(operation.release(), Impl::releaseIUnknown);
	program->rootSignatures.reserve(impl_->rootSignatures_.size());
	for (auto &rootSignature : impl_->rootSignatures_)
		program->rootSignatures.emplace_back(rootSignature.release(), Impl::releaseIUnknown);
	program->pipelines.reserve(impl_->pipelines_.size());
	for (auto &pipeline : impl_->pipelines_)
		program->pipelines.emplace_back(pipeline.release(), Impl::releaseIUnknown);
	program->weightUpload = {impl_->weightUpload_.release(), Impl::releaseIUnknown};
	return program;
}

void DmlProgramCompiler::Impl::describeInitializationInputs(std::vector<DML_BINDING_DESC> &inputs)
{
	for (std::size_t nodeIndex = 0; nodeIndex < operations_.size(); ++nodeIndex) {
		if (!operations_[nodeIndex])
			continue;
		const auto &step = definition_.params.steps[nodeIndex];
		auto &buffers = initializationInputBuffers_[nodeIndex];
		for (std::size_t input = 0; input < step.inputCount; ++input) {
			const auto tensorIndex = step.inputs[input];
			if (tensorIndex == -1 || !isWeightTensor(definition_.params.tensors[tensorIndex]))
				continue;
			const auto &tensor = definition_.params.tensors[tensorIndex];
			buffers[input] = DML_BUFFER_BINDING{weight_.get(),
							    static_cast<std::uint64_t>(tensorByteOffset(tensor)),
							    tensorSize(tensor)};
		}
		initializationInputArrays_[nodeIndex] = {static_cast<UINT>(buffers.size()), buffers.data()};
		inputs.push_back({DML_BINDING_TYPE_BUFFER_ARRAY, &initializationInputArrays_[nodeIndex]});
	}
}

void DmlProgramCompiler::Impl::describeInitializationOutputs(std::vector<DML_BINDING_DESC> &outputs)
{
	for (std::size_t nodeIndex = 0; nodeIndex < operations_.size(); ++nodeIndex) {
		if (!operations_[nodeIndex])
			continue;
		if (const auto &persistentResource = dmlPersistentResources_[nodeIndex]; persistentResource) {
			initializationPersistentBuffers_[nodeIndex] = DML_BUFFER_BINDING{
				persistentResource.get(), 0,
				operations_[nodeIndex]->GetBindingProperties().PersistentResourceSize};
			outputs.push_back({DML_BINDING_TYPE_BUFFER, &initializationPersistentBuffers_[nodeIndex]});
		} else {
			outputs.push_back({DML_BINDING_TYPE_NONE, nullptr});
		}
	}
}

auto DmlProgramCompiler::Impl::signalCompletion() const noexcept -> HRESULT
{
	const HRESULT signal = queue_->Signal(fence_.get(), 1);
	if (FAILED(signal) && FAILED(queue_->Signal(fence_.get(), 1)) &&
	    SUCCEEDED(d3dDevice_->GetDeviceRemovedReason()))
		std::terminate(); // A live queue with no completion marker cannot safely release its resources.
	return signal;
}

void DmlProgramCompiler::Impl::waitForCompletion() const
{
	// After submission, only completion or device removal permits resource destruction.
	for (;;) {
		const auto completed = fence_->GetCompletedValue();
		if (completed == std::numeric_limits<std::uint64_t>::max())
			throw DmlProgramCompilerError("Compiler GPU device removed",
						      d3dDevice_->GetDeviceRemovedReason());
		if (completed >= 1)
			break;
		if (const HRESULT result = d3dDevice_->GetDeviceRemovedReason(); FAILED(result))
			throw DmlProgramCompilerError("Compiler GPU completion failed", result);
		Sleep(1);
	}
}

// MARK: - Node visitors (CreateOperator and CompileOperator are written per node)

// Input resources use COPY_DEST; weight nodes share the packed weight resource.
auto DmlProgramCompiler::operator()(const InputItem &) -> void
{
	const auto nodeIndex = impl_->nodeIndex_;
	if (isWeightTensor(impl_->definition_.params.tensors[nodeIndex]))
		impl_->tensorResources_[nodeIndex] = impl_->weight_;
	else
		impl_->tensorResources_[nodeIndex] =
			impl_->makeNodeTensorResource(nodeIndex, D3D12_RESOURCE_STATE_COPY_DEST);
}

auto DmlProgramCompiler::operator()(const BiasedConv2dRelu &node) -> void
{
	impl_->tensorResources_[impl_->nodeIndex_] =
		impl_->makeNodeTensorResource(impl_->nodeIndex_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	const auto &definition = impl_->definition_.params.steps[impl_->nodeIndex_];
	const auto tensors = impl_->definition_.params.tensors;
	const auto input = definition.inputs[0];
	const auto filter = definition.inputs[1];
	const auto bias = definition.inputs[2];
	const auto output = definition.output;
	const auto inDescBuffer = bufferTensorDesc(tensors[input]);
	const DML_TENSOR_DESC inDesc{DML_TENSOR_TYPE_BUFFER, &inDescBuffer};
	const auto filterDescBuffer = bufferTensorDesc(tensors[filter]);
	const DML_TENSOR_DESC filterDesc{DML_TENSOR_TYPE_BUFFER, &filterDescBuffer};
	const auto outDescBuffer = bufferTensorDesc(tensors[output]);
	const DML_TENSOR_DESC outDesc{DML_TENSOR_TYPE_BUFFER, &outDescBuffer};
	// Conv's rank-one bias uses an SDK-only rank-four descriptor.
	const auto &biasSpec = tensors[bias];
	const std::array<UINT, 4> biasSizes{1, tensorSizes(biasSpec)[0], 1, 1};
	const std::array<UINT, 4> biasStrides{tensorSizes(biasSpec)[0], 1, 1, 1};
	auto biasBuffer = bufferTensorDesc(biasSpec);
	biasBuffer.DimensionCount = 4;
	biasBuffer.Sizes = biasSizes.data();
	biasBuffer.Strides = biasStrides.data();
	const DML_TENSOR_DESC biasDesc{DML_TENSOR_TYPE_BUFFER, &biasBuffer};
	const std::array<UINT, 2> outputPadding{};
	DML_CONVOLUTION_OPERATOR_DESC desc{};
	desc.InputTensor = &inDesc;
	desc.FilterTensor = &filterDesc;
	desc.BiasTensor = &biasDesc;
	desc.OutputTensor = &outDesc;
	desc.Mode = DML_CONVOLUTION_MODE_CROSS_CORRELATION;
	desc.Direction = DML_CONVOLUTION_DIRECTION_FORWARD;
	desc.DimensionCount = 2;
	desc.Strides = node.strides.data();
	desc.Dilations = node.dilations.data();
	desc.StartPadding = node.startPadding.data();
	desc.EndPadding = node.endPadding.data();
	desc.OutputPadding = outputPadding.data();
	desc.GroupCount = node.groupCount;
	DML_ACTIVATION_RELU_OPERATOR_DESC relu{};
	DML_OPERATOR_DESC activation{DML_OPERATOR_ACTIVATION_RELU, &relu};
	desc.FusedActivation = &activation;
	const DML_OPERATOR_DESC operatorDesc{DML_OPERATOR_CONVOLUTION, &desc};
	std::unique_ptr<IDMLOperator, IUnknownReleaser> operation;
	IDMLOperator *operationPointer = nullptr;
	const HRESULT operationResult =
		impl_->dmlDevice_->CreateOperator(&operatorDesc, IID_PPV_ARGS(&operationPointer));
	operation.reset(operationPointer);
	if (const HRESULT result = operationResult; FAILED(result))
		throw DmlProgramCompilerError("CreateOperator failed", result);

	std::unique_ptr<IDMLCompiledOperator, IUnknownReleaser> compiledOperation;
	IDMLCompiledOperator *compiledOperationPointer = nullptr;
	const HRESULT compiledOperationResult = impl_->dmlDevice_->CompileOperator(
		operation.get(), DML_EXECUTION_FLAG_NONE, IID_PPV_ARGS(&compiledOperationPointer));
	compiledOperation.reset(compiledOperationPointer);
	if (const HRESULT result = compiledOperationResult; FAILED(result))
		throw DmlProgramCompilerError("CompileOperator failed", result);
	impl_->operations_[impl_->nodeIndex_] = std::move(compiledOperation);
	impl_->dmlPersistentResources_[impl_->nodeIndex_] =
		makeDmlPersistentResource(impl_->d3dDevice_.get(), impl_->operations_[impl_->nodeIndex_].get());
	impl_->operatorsToReset_.push_back(impl_->operations_[impl_->nodeIndex_].get());
}

auto DmlProgramCompiler::operator()(const HardSwishGate &node) -> void
{
	impl_->tensorResources_[impl_->nodeIndex_] =
		impl_->makeNodeTensorResource(impl_->nodeIndex_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	const auto &definition = impl_->definition_.params.steps[impl_->nodeIndex_];
	const auto tensors = impl_->definition_.params.tensors;
	const auto input = definition.inputs[0];
	const auto output = definition.output;
	const auto inDescBuffer = bufferTensorDesc(tensors[input]);
	const DML_TENSOR_DESC inDesc{DML_TENSOR_TYPE_BUFFER, &inDescBuffer};
	const auto outDescBuffer = bufferTensorDesc(tensors[output]);
	const DML_TENSOR_DESC outDesc{DML_TENSOR_TYPE_BUFFER, &outDescBuffer};
	DML_ACTIVATION_HARD_SIGMOID_OPERATOR_DESC desc{&inDesc, &outDesc, 1.0F / 6.0F, 0.5F};
	const DML_OPERATOR_DESC operatorDesc{DML_OPERATOR_ACTIVATION_HARD_SIGMOID, &desc};
	std::unique_ptr<IDMLOperator, IUnknownReleaser> operation;
	IDMLOperator *operationPointer = nullptr;
	const HRESULT operationResult =
		impl_->dmlDevice_->CreateOperator(&operatorDesc, IID_PPV_ARGS(&operationPointer));
	operation.reset(operationPointer);
	if (const HRESULT result = operationResult; FAILED(result))
		throw DmlProgramCompilerError("CreateOperator failed", result);

	std::unique_ptr<IDMLCompiledOperator, IUnknownReleaser> compiledOperation;
	IDMLCompiledOperator *compiledOperationPointer = nullptr;
	const HRESULT compiledOperationResult = impl_->dmlDevice_->CompileOperator(
		operation.get(), DML_EXECUTION_FLAG_NONE, IID_PPV_ARGS(&compiledOperationPointer));
	compiledOperation.reset(compiledOperationPointer);
	if (const HRESULT result = compiledOperationResult; FAILED(result))
		throw DmlProgramCompilerError("CompileOperator failed", result);
	impl_->operations_[impl_->nodeIndex_] = std::move(compiledOperation);
	impl_->dmlPersistentResources_[impl_->nodeIndex_] =
		makeDmlPersistentResource(impl_->d3dDevice_.get(), impl_->operations_[impl_->nodeIndex_].get());
	impl_->operatorsToReset_.push_back(impl_->operations_[impl_->nodeIndex_].get());
}

// Custom shader resources and output tensor are created by the node visitor.
auto DmlProgramCompiler::operator()(const MPLD10DepthwiseConv2dRelu &) -> void
{
	kDepthwiseConvShader(impl_->d3dDevice_.get(), impl_->rootSignatures_[impl_->nodeIndex_],
			     impl_->pipelines_[impl_->nodeIndex_]);
	impl_->tensorResources_[impl_->nodeIndex_] =
		impl_->makeNodeTensorResource(impl_->nodeIndex_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

auto DmlProgramCompiler::operator()(const MPLD10BinarizeFloat32 &) -> void
{
	kMaskShader(impl_->d3dDevice_.get(), impl_->rootSignatures_[impl_->nodeIndex_],
		    impl_->pipelines_[impl_->nodeIndex_]);
	impl_->tensorResources_[impl_->nodeIndex_] =
		impl_->makeNodeTensorResource(impl_->nodeIndex_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

auto DmlProgramCompiler::operator()(const AveragePool2d &node) -> void
{
	impl_->tensorResources_[impl_->nodeIndex_] =
		impl_->makeNodeTensorResource(impl_->nodeIndex_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	const auto &definition = impl_->definition_.params.steps[impl_->nodeIndex_];
	const auto tensors = impl_->definition_.params.tensors;
	const auto input = definition.inputs[0];
	const auto output = definition.output;
	const auto inDescBuffer = bufferTensorDesc(tensors[input]);
	const DML_TENSOR_DESC inDesc{DML_TENSOR_TYPE_BUFFER, &inDescBuffer};
	const auto outDescBuffer = bufferTensorDesc(tensors[output]);
	const DML_TENSOR_DESC outDesc{DML_TENSOR_TYPE_BUFFER, &outDescBuffer};
	DML_AVERAGE_POOLING_OPERATOR_DESC desc{};
	desc.InputTensor = &inDesc;
	desc.OutputTensor = &outDesc;
	desc.DimensionCount = 2;
	desc.Strides = node.strides.data();
	desc.WindowSize = node.window.data();
	desc.StartPadding = node.startPadding.data();
	desc.EndPadding = node.endPadding.data();
	desc.IncludePadding = FALSE;
	const DML_OPERATOR_DESC operatorDesc{DML_OPERATOR_AVERAGE_POOLING, &desc};
	std::unique_ptr<IDMLOperator, IUnknownReleaser> operation;
	IDMLOperator *operationPointer = nullptr;
	const HRESULT operationResult =
		impl_->dmlDevice_->CreateOperator(&operatorDesc, IID_PPV_ARGS(&operationPointer));
	operation.reset(operationPointer);
	if (const HRESULT result = operationResult; FAILED(result))
		throw DmlProgramCompilerError("CreateOperator failed", result);

	std::unique_ptr<IDMLCompiledOperator, IUnknownReleaser> compiledOperation;
	IDMLCompiledOperator *compiledOperationPointer = nullptr;
	const HRESULT compiledOperationResult = impl_->dmlDevice_->CompileOperator(
		operation.get(), DML_EXECUTION_FLAG_NONE, IID_PPV_ARGS(&compiledOperationPointer));
	compiledOperation.reset(compiledOperationPointer);
	if (const HRESULT result = compiledOperationResult; FAILED(result))
		throw DmlProgramCompilerError("CompileOperator failed", result);
	impl_->operations_[impl_->nodeIndex_] = std::move(compiledOperation);
	impl_->dmlPersistentResources_[impl_->nodeIndex_] =
		makeDmlPersistentResource(impl_->d3dDevice_.get(), impl_->operations_[impl_->nodeIndex_].get());
	impl_->operatorsToReset_.push_back(impl_->operations_[impl_->nodeIndex_].get());
}

auto DmlProgramCompiler::operator()(const BiasedConv2d &node) -> void
{
	impl_->tensorResources_[impl_->nodeIndex_] =
		impl_->makeNodeTensorResource(impl_->nodeIndex_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	const auto &definition = impl_->definition_.params.steps[impl_->nodeIndex_];
	const auto tensors = impl_->definition_.params.tensors;
	const auto input = definition.inputs[0];
	const auto filter = definition.inputs[1];
	const auto bias = definition.inputs[2];
	const auto output = definition.output;
	const auto inDescBuffer = bufferTensorDesc(tensors[input]);
	const DML_TENSOR_DESC inDesc{DML_TENSOR_TYPE_BUFFER, &inDescBuffer};
	const auto filterDescBuffer = bufferTensorDesc(tensors[filter]);
	const DML_TENSOR_DESC filterDesc{DML_TENSOR_TYPE_BUFFER, &filterDescBuffer};
	const auto outDescBuffer = bufferTensorDesc(tensors[output]);
	const DML_TENSOR_DESC outDesc{DML_TENSOR_TYPE_BUFFER, &outDescBuffer};
	// Conv's rank-one bias uses an SDK-only rank-four descriptor.
	const auto &biasSpec = tensors[bias];
	const std::array<UINT, 4> biasSizes{1, tensorSizes(biasSpec)[0], 1, 1};
	const std::array<UINT, 4> biasStrides{tensorSizes(biasSpec)[0], 1, 1, 1};
	auto biasBuffer = bufferTensorDesc(biasSpec);
	biasBuffer.DimensionCount = 4;
	biasBuffer.Sizes = biasSizes.data();
	biasBuffer.Strides = biasStrides.data();
	const DML_TENSOR_DESC biasDesc{DML_TENSOR_TYPE_BUFFER, &biasBuffer};
	const std::array<UINT, 2> outputPadding{};
	DML_CONVOLUTION_OPERATOR_DESC desc{};
	desc.InputTensor = &inDesc;
	desc.FilterTensor = &filterDesc;
	desc.BiasTensor = &biasDesc;
	desc.OutputTensor = &outDesc;
	desc.Mode = DML_CONVOLUTION_MODE_CROSS_CORRELATION;
	desc.Direction = DML_CONVOLUTION_DIRECTION_FORWARD;
	desc.DimensionCount = 2;
	desc.Strides = node.strides.data();
	desc.Dilations = node.dilations.data();
	desc.StartPadding = node.startPadding.data();
	desc.EndPadding = node.endPadding.data();
	desc.OutputPadding = outputPadding.data();
	desc.GroupCount = node.groupCount;

	const DML_OPERATOR_DESC operatorDesc{DML_OPERATOR_CONVOLUTION, &desc};
	std::unique_ptr<IDMLOperator, IUnknownReleaser> operation;
	IDMLOperator *operationPointer = nullptr;
	const HRESULT operationResult =
		impl_->dmlDevice_->CreateOperator(&operatorDesc, IID_PPV_ARGS(&operationPointer));
	operation.reset(operationPointer);
	if (const HRESULT result = operationResult; FAILED(result))
		throw DmlProgramCompilerError("CreateOperator failed", result);

	std::unique_ptr<IDMLCompiledOperator, IUnknownReleaser> compiledOperation;
	IDMLCompiledOperator *compiledOperationPointer = nullptr;
	const HRESULT compiledOperationResult = impl_->dmlDevice_->CompileOperator(
		operation.get(), DML_EXECUTION_FLAG_NONE, IID_PPV_ARGS(&compiledOperationPointer));
	compiledOperation.reset(compiledOperationPointer);
	if (const HRESULT result = compiledOperationResult; FAILED(result))
		throw DmlProgramCompilerError("CompileOperator failed", result);
	impl_->operations_[impl_->nodeIndex_] = std::move(compiledOperation);
	impl_->dmlPersistentResources_[impl_->nodeIndex_] =
		makeDmlPersistentResource(impl_->d3dDevice_.get(), impl_->operations_[impl_->nodeIndex_].get());
	impl_->operatorsToReset_.push_back(impl_->operations_[impl_->nodeIndex_].get());
}

auto DmlProgramCompiler::operator()(const BiasedConvTranspose2d &node) -> void
{
	impl_->tensorResources_[impl_->nodeIndex_] =
		impl_->makeNodeTensorResource(impl_->nodeIndex_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	const auto &definition = impl_->definition_.params.steps[impl_->nodeIndex_];
	const auto tensors = impl_->definition_.params.tensors;
	const auto input = definition.inputs[0];
	const auto filter = definition.inputs[1];
	const auto bias = definition.inputs[2];
	const auto output = definition.output;
	const auto inDescBuffer = bufferTensorDesc(tensors[input]);
	const DML_TENSOR_DESC inDesc{DML_TENSOR_TYPE_BUFFER, &inDescBuffer};
	const auto filterDescBuffer = bufferTensorDesc(tensors[filter]);
	const DML_TENSOR_DESC filterDesc{DML_TENSOR_TYPE_BUFFER, &filterDescBuffer};
	const auto outDescBuffer = bufferTensorDesc(tensors[output]);
	const DML_TENSOR_DESC outDesc{DML_TENSOR_TYPE_BUFFER, &outDescBuffer};
	// Conv's rank-one bias uses an SDK-only rank-four descriptor.
	const auto &biasSpec = tensors[bias];
	const std::array<UINT, 4> biasSizes{1, tensorSizes(biasSpec)[0], 1, 1};
	const std::array<UINT, 4> biasStrides{tensorSizes(biasSpec)[0], 1, 1, 1};
	auto biasBuffer = bufferTensorDesc(biasSpec);
	biasBuffer.DimensionCount = 4;
	biasBuffer.Sizes = biasSizes.data();
	biasBuffer.Strides = biasStrides.data();
	const DML_TENSOR_DESC biasDesc{DML_TENSOR_TYPE_BUFFER, &biasBuffer};
	const std::array<UINT, 2> outputPadding{};
	DML_CONVOLUTION_OPERATOR_DESC desc{};
	desc.InputTensor = &inDesc;
	desc.FilterTensor = &filterDesc;
	desc.BiasTensor = &biasDesc;
	desc.OutputTensor = &outDesc;
	desc.Mode = DML_CONVOLUTION_MODE_CROSS_CORRELATION;
	desc.Direction = DML_CONVOLUTION_DIRECTION_BACKWARD;
	desc.DimensionCount = 2;
	desc.Strides = node.strides.data();
	desc.Dilations = node.dilations.data();
	desc.StartPadding = node.startPadding.data();
	desc.EndPadding = node.endPadding.data();
	desc.OutputPadding = outputPadding.data();
	desc.GroupCount = node.groupCount;

	const DML_OPERATOR_DESC operatorDesc{DML_OPERATOR_CONVOLUTION, &desc};
	std::unique_ptr<IDMLOperator, IUnknownReleaser> operation;
	IDMLOperator *operationPointer = nullptr;
	const HRESULT operationResult =
		impl_->dmlDevice_->CreateOperator(&operatorDesc, IID_PPV_ARGS(&operationPointer));
	operation.reset(operationPointer);
	if (const HRESULT result = operationResult; FAILED(result))
		throw DmlProgramCompilerError("CreateOperator failed", result);

	std::unique_ptr<IDMLCompiledOperator, IUnknownReleaser> compiledOperation;
	IDMLCompiledOperator *compiledOperationPointer = nullptr;
	const HRESULT compiledOperationResult = impl_->dmlDevice_->CompileOperator(
		operation.get(), DML_EXECUTION_FLAG_NONE, IID_PPV_ARGS(&compiledOperationPointer));
	compiledOperation.reset(compiledOperationPointer);
	if (const HRESULT result = compiledOperationResult; FAILED(result))
		throw DmlProgramCompilerError("CompileOperator failed", result);
	impl_->operations_[impl_->nodeIndex_] = std::move(compiledOperation);
	impl_->dmlPersistentResources_[impl_->nodeIndex_] =
		makeDmlPersistentResource(impl_->d3dDevice_.get(), impl_->operations_[impl_->nodeIndex_].get());
	impl_->operatorsToReset_.push_back(impl_->operations_[impl_->nodeIndex_].get());
}

auto DmlProgramCompiler::operator()(const DepthToSpace &node) -> void
{
	impl_->tensorResources_[impl_->nodeIndex_] =
		impl_->makeNodeTensorResource(impl_->nodeIndex_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	const auto &definition = impl_->definition_.params.steps[impl_->nodeIndex_];
	const auto tensors = impl_->definition_.params.tensors;
	const auto input = definition.inputs[0];
	const auto output = definition.output;
	const auto xdBuffer = bufferTensorDesc(tensors[input]);
	const DML_TENSOR_DESC xd{DML_TENSOR_TYPE_BUFFER, &xdBuffer};
	const auto ydBuffer = bufferTensorDesc(tensors[output]);
	const DML_TENSOR_DESC yd{DML_TENSOR_TYPE_BUFFER, &ydBuffer};
	DML_DEPTH_TO_SPACE_OPERATOR_DESC desc{&xd, &yd, node.blockSize};
	const DML_OPERATOR_DESC operatorDesc{DML_OPERATOR_DEPTH_TO_SPACE, &desc};
	std::unique_ptr<IDMLOperator, IUnknownReleaser> operation;
	IDMLOperator *operationPointer = nullptr;
	const HRESULT operationResult =
		impl_->dmlDevice_->CreateOperator(&operatorDesc, IID_PPV_ARGS(&operationPointer));
	operation.reset(operationPointer);
	if (const HRESULT result = operationResult; FAILED(result))
		throw DmlProgramCompilerError("CreateOperator failed", result);

	std::unique_ptr<IDMLCompiledOperator, IUnknownReleaser> compiledOperation;
	IDMLCompiledOperator *compiledOperationPointer = nullptr;
	const HRESULT compiledOperationResult = impl_->dmlDevice_->CompileOperator(
		operation.get(), DML_EXECUTION_FLAG_NONE, IID_PPV_ARGS(&compiledOperationPointer));
	compiledOperation.reset(compiledOperationPointer);
	if (const HRESULT result = compiledOperationResult; FAILED(result))
		throw DmlProgramCompilerError("CompileOperator failed", result);
	impl_->operations_[impl_->nodeIndex_] = std::move(compiledOperation);
	impl_->dmlPersistentResources_[impl_->nodeIndex_] =
		makeDmlPersistentResource(impl_->d3dDevice_.get(), impl_->operations_[impl_->nodeIndex_].get());
	impl_->operatorsToReset_.push_back(impl_->operations_[impl_->nodeIndex_].get());
}

auto DmlProgramCompiler::operator()(const ClipFloat32 &node) -> void
{
	impl_->tensorResources_[impl_->nodeIndex_] =
		impl_->makeNodeTensorResource(impl_->nodeIndex_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	const auto &definition = impl_->definition_.params.steps[impl_->nodeIndex_];
	const auto tensors = impl_->definition_.params.tensors;
	const auto input = definition.inputs[0];
	const auto output = definition.output;
	const auto inDescBuffer = bufferTensorDesc(tensors[input]);
	const DML_TENSOR_DESC inDesc{DML_TENSOR_TYPE_BUFFER, &inDescBuffer};
	const auto outDescBuffer = bufferTensorDesc(tensors[output]);
	const DML_TENSOR_DESC outDesc{DML_TENSOR_TYPE_BUFFER, &outDescBuffer};
	DML_ELEMENT_WISE_CLIP_OPERATOR_DESC desc{};
	desc.InputTensor = &inDesc;
	desc.OutputTensor = &outDesc;
	desc.Min = node.minimum;
	desc.Max = node.maximum;
	const DML_OPERATOR_DESC operatorDesc{DML_OPERATOR_ELEMENT_WISE_CLIP, &desc};
	std::unique_ptr<IDMLOperator, IUnknownReleaser> operation;
	IDMLOperator *operationPointer = nullptr;
	const HRESULT operationResult =
		impl_->dmlDevice_->CreateOperator(&operatorDesc, IID_PPV_ARGS(&operationPointer));
	operation.reset(operationPointer);
	if (const HRESULT result = operationResult; FAILED(result))
		throw DmlProgramCompilerError("CreateOperator failed", result);

	std::unique_ptr<IDMLCompiledOperator, IUnknownReleaser> compiledOperation;
	IDMLCompiledOperator *compiledOperationPointer = nullptr;
	const HRESULT compiledOperationResult = impl_->dmlDevice_->CompileOperator(
		operation.get(), DML_EXECUTION_FLAG_NONE, IID_PPV_ARGS(&compiledOperationPointer));
	compiledOperation.reset(compiledOperationPointer);
	if (const HRESULT result = compiledOperationResult; FAILED(result))
		throw DmlProgramCompilerError("CompileOperator failed", result);
	impl_->operations_[impl_->nodeIndex_] = std::move(compiledOperation);
	impl_->dmlPersistentResources_[impl_->nodeIndex_] =
		makeDmlPersistentResource(impl_->d3dDevice_.get(), impl_->operations_[impl_->nodeIndex_].get());
	impl_->operatorsToReset_.push_back(impl_->operations_[impl_->nodeIndex_].get());
}

auto DmlProgramCompiler::operator()(const Resize &node) -> void
{
	impl_->tensorResources_[impl_->nodeIndex_] =
		impl_->makeNodeTensorResource(impl_->nodeIndex_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	const auto &definition = impl_->definition_.params.steps[impl_->nodeIndex_];
	const auto tensors = impl_->definition_.params.tensors;
	const auto input = definition.inputs[0];
	const auto output = definition.output;
	const auto inDescBuffer = bufferTensorDesc(tensors[input]);
	const DML_TENSOR_DESC inDesc{DML_TENSOR_TYPE_BUFFER, &inDescBuffer};
	const auto outDescBuffer = bufferTensorDesc(tensors[output]);
	const DML_TENSOR_DESC outDesc{DML_TENSOR_TYPE_BUFFER, &outDescBuffer};
	DML_UPSAMPLE_2D_OPERATOR_DESC desc{};
	desc.InputTensor = &inDesc;
	desc.OutputTensor = &outDesc;
	desc.ScaleSize = {node.width, node.height};
	desc.InterpolationMode = DML_INTERPOLATION_MODE_LINEAR;
	const DML_OPERATOR_DESC operatorDesc{DML_OPERATOR_UPSAMPLE_2D, &desc};
	std::unique_ptr<IDMLOperator, IUnknownReleaser> operation;
	IDMLOperator *operationPointer = nullptr;
	const HRESULT operationResult =
		impl_->dmlDevice_->CreateOperator(&operatorDesc, IID_PPV_ARGS(&operationPointer));
	operation.reset(operationPointer);
	if (const HRESULT result = operationResult; FAILED(result))
		throw DmlProgramCompilerError("CreateOperator failed", result);

	std::unique_ptr<IDMLCompiledOperator, IUnknownReleaser> compiledOperation;
	IDMLCompiledOperator *compiledOperationPointer = nullptr;
	const HRESULT compiledOperationResult = impl_->dmlDevice_->CompileOperator(
		operation.get(), DML_EXECUTION_FLAG_NONE, IID_PPV_ARGS(&compiledOperationPointer));
	compiledOperation.reset(compiledOperationPointer);
	if (const HRESULT result = compiledOperationResult; FAILED(result))
		throw DmlProgramCompilerError("CompileOperator failed", result);
	impl_->operations_[impl_->nodeIndex_] = std::move(compiledOperation);
	impl_->dmlPersistentResources_[impl_->nodeIndex_] =
		makeDmlPersistentResource(impl_->d3dDevice_.get(), impl_->operations_[impl_->nodeIndex_].get());
	impl_->operatorsToReset_.push_back(impl_->operations_[impl_->nodeIndex_].get());
}

auto DmlProgramCompiler::operator()(const Sigmoid &node) -> void
{
	impl_->tensorResources_[impl_->nodeIndex_] =
		impl_->makeNodeTensorResource(impl_->nodeIndex_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	const auto &definition = impl_->definition_.params.steps[impl_->nodeIndex_];
	const auto tensors = impl_->definition_.params.tensors;
	const auto input = definition.inputs[0];
	const auto output = definition.output;
	const auto inDescBuffer = bufferTensorDesc(tensors[input]);
	const DML_TENSOR_DESC inDesc{DML_TENSOR_TYPE_BUFFER, &inDescBuffer};
	const auto outDescBuffer = bufferTensorDesc(tensors[output]);
	const DML_TENSOR_DESC outDesc{DML_TENSOR_TYPE_BUFFER, &outDescBuffer};
	DML_ACTIVATION_SIGMOID_OPERATOR_DESC desc{};
	desc.InputTensor = &inDesc;
	desc.OutputTensor = &outDesc;
	const DML_OPERATOR_DESC operatorDesc{DML_OPERATOR_ACTIVATION_SIGMOID, &desc};
	std::unique_ptr<IDMLOperator, IUnknownReleaser> operation;
	IDMLOperator *operationPointer = nullptr;
	const HRESULT operationResult =
		impl_->dmlDevice_->CreateOperator(&operatorDesc, IID_PPV_ARGS(&operationPointer));
	operation.reset(operationPointer);
	if (const HRESULT result = operationResult; FAILED(result))
		throw DmlProgramCompilerError("CreateOperator failed", result);

	std::unique_ptr<IDMLCompiledOperator, IUnknownReleaser> compiledOperation;
	IDMLCompiledOperator *compiledOperationPointer = nullptr;
	const HRESULT compiledOperationResult = impl_->dmlDevice_->CompileOperator(
		operation.get(), DML_EXECUTION_FLAG_NONE, IID_PPV_ARGS(&compiledOperationPointer));
	compiledOperation.reset(compiledOperationPointer);
	if (const HRESULT result = compiledOperationResult; FAILED(result))
		throw DmlProgramCompilerError("CompileOperator failed", result);
	impl_->operations_[impl_->nodeIndex_] = std::move(compiledOperation);
	impl_->dmlPersistentResources_[impl_->nodeIndex_] =
		makeDmlPersistentResource(impl_->d3dDevice_.get(), impl_->operations_[impl_->nodeIndex_].get());
	impl_->operatorsToReset_.push_back(impl_->operations_[impl_->nodeIndex_].get());
}

auto DmlProgramCompiler::operator()(const Reshape &node) -> void
{
	impl_->tensorResources_[impl_->nodeIndex_] = impl_->tensorResources_[node.inputIndex];
}

auto DmlProgramCompiler::operator()(const Expand &node) -> void
{
	impl_->tensorResources_[impl_->nodeIndex_] = impl_->tensorResources_[node.inputIndex];
}

auto DmlProgramCompiler::operator()(const Add &node) -> void
{
	impl_->tensorResources_[impl_->nodeIndex_] =
		impl_->makeNodeTensorResource(impl_->nodeIndex_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	const auto &definition = impl_->definition_.params.steps[impl_->nodeIndex_];
	const auto tensors = impl_->definition_.params.tensors;
	const auto left = definition.inputs[0];
	const auto right = definition.inputs[1];
	const auto output = definition.output;
	const auto leftDescBuffer = bufferTensorDesc(tensors[left]);
	const DML_TENSOR_DESC leftDesc{DML_TENSOR_TYPE_BUFFER, &leftDescBuffer};
	const auto rightDescBuffer = bufferTensorDesc(tensors[right]);
	const DML_TENSOR_DESC rightDesc{DML_TENSOR_TYPE_BUFFER, &rightDescBuffer};
	const auto outputDescBuffer = bufferTensorDesc(tensors[output]);
	const DML_TENSOR_DESC outputDesc{DML_TENSOR_TYPE_BUFFER, &outputDescBuffer};
	DML_ELEMENT_WISE_ADD_OPERATOR_DESC desc{&leftDesc, &rightDesc, &outputDesc};
	const DML_OPERATOR_DESC operatorDesc{DML_OPERATOR_ELEMENT_WISE_ADD, &desc};
	std::unique_ptr<IDMLOperator, IUnknownReleaser> operation;
	IDMLOperator *operationPointer = nullptr;
	const HRESULT operationResult =
		impl_->dmlDevice_->CreateOperator(&operatorDesc, IID_PPV_ARGS(&operationPointer));
	operation.reset(operationPointer);
	if (const HRESULT result = operationResult; FAILED(result))
		throw DmlProgramCompilerError("CreateOperator failed", result);

	std::unique_ptr<IDMLCompiledOperator, IUnknownReleaser> compiledOperation;
	IDMLCompiledOperator *compiledOperationPointer = nullptr;
	const HRESULT compiledOperationResult = impl_->dmlDevice_->CompileOperator(
		operation.get(), DML_EXECUTION_FLAG_NONE, IID_PPV_ARGS(&compiledOperationPointer));
	compiledOperation.reset(compiledOperationPointer);
	if (const HRESULT result = compiledOperationResult; FAILED(result))
		throw DmlProgramCompilerError("CompileOperator failed", result);
	impl_->operations_[impl_->nodeIndex_] = std::move(compiledOperation);
	impl_->dmlPersistentResources_[impl_->nodeIndex_] =
		makeDmlPersistentResource(impl_->d3dDevice_.get(), impl_->operations_[impl_->nodeIndex_].get());
	impl_->operatorsToReset_.push_back(impl_->operations_[impl_->nodeIndex_].get());
}

auto DmlProgramCompiler::operator()(const Mul &node) -> void
{
	impl_->tensorResources_[impl_->nodeIndex_] =
		impl_->makeNodeTensorResource(impl_->nodeIndex_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	const auto &definition = impl_->definition_.params.steps[impl_->nodeIndex_];
	const auto tensors = impl_->definition_.params.tensors;
	const auto left = definition.inputs[0];
	const auto right = definition.inputs[1];
	const auto output = definition.output;
	const auto leftDescBuffer = bufferTensorDesc(tensors[left]);
	const DML_TENSOR_DESC leftDesc{DML_TENSOR_TYPE_BUFFER, &leftDescBuffer};
	const auto rightDescBuffer = bufferTensorDesc(tensors[right]);
	const DML_TENSOR_DESC rightDesc{DML_TENSOR_TYPE_BUFFER, &rightDescBuffer};
	const auto outputDescBuffer = bufferTensorDesc(tensors[output]);
	const DML_TENSOR_DESC outputDesc{DML_TENSOR_TYPE_BUFFER, &outputDescBuffer};
	DML_ELEMENT_WISE_MULTIPLY_OPERATOR_DESC desc{&leftDesc, &rightDesc, &outputDesc};
	const DML_OPERATOR_DESC operatorDesc{DML_OPERATOR_ELEMENT_WISE_MULTIPLY, &desc};
	std::unique_ptr<IDMLOperator, IUnknownReleaser> operation;
	IDMLOperator *operationPointer = nullptr;
	const HRESULT operationResult =
		impl_->dmlDevice_->CreateOperator(&operatorDesc, IID_PPV_ARGS(&operationPointer));
	operation.reset(operationPointer);
	if (const HRESULT result = operationResult; FAILED(result))
		throw DmlProgramCompilerError("CreateOperator failed", result);

	std::unique_ptr<IDMLCompiledOperator, IUnknownReleaser> compiledOperation;
	IDMLCompiledOperator *compiledOperationPointer = nullptr;
	const HRESULT compiledOperationResult = impl_->dmlDevice_->CompileOperator(
		operation.get(), DML_EXECUTION_FLAG_NONE, IID_PPV_ARGS(&compiledOperationPointer));
	compiledOperation.reset(compiledOperationPointer);
	if (const HRESULT result = compiledOperationResult; FAILED(result))
		throw DmlProgramCompilerError("CompileOperator failed", result);
	impl_->operations_[impl_->nodeIndex_] = std::move(compiledOperation);
	impl_->dmlPersistentResources_[impl_->nodeIndex_] =
		makeDmlPersistentResource(impl_->d3dDevice_.get(), impl_->operations_[impl_->nodeIndex_].get());
	impl_->operatorsToReset_.push_back(impl_->operations_[impl_->nodeIndex_].get());
}

auto DmlProgramCompiler::operator()(const Relu &node) -> void
{
	impl_->tensorResources_[impl_->nodeIndex_] =
		impl_->makeNodeTensorResource(impl_->nodeIndex_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	const auto &definition = impl_->definition_.params.steps[impl_->nodeIndex_];
	const auto tensors = impl_->definition_.params.tensors;
	const auto input = definition.inputs[0];
	const auto output = definition.output;
	const auto inDescBuffer = bufferTensorDesc(tensors[input]);
	const DML_TENSOR_DESC inDesc{DML_TENSOR_TYPE_BUFFER, &inDescBuffer};
	const auto outDescBuffer = bufferTensorDesc(tensors[output]);
	const DML_TENSOR_DESC outDesc{DML_TENSOR_TYPE_BUFFER, &outDescBuffer};
	DML_ACTIVATION_RELU_OPERATOR_DESC desc{};
	desc.InputTensor = &inDesc;
	desc.OutputTensor = &outDesc;
	const DML_OPERATOR_DESC operatorDesc{DML_OPERATOR_ACTIVATION_RELU, &desc};
	std::unique_ptr<IDMLOperator, IUnknownReleaser> operation;
	IDMLOperator *operationPointer = nullptr;
	const HRESULT operationResult =
		impl_->dmlDevice_->CreateOperator(&operatorDesc, IID_PPV_ARGS(&operationPointer));
	operation.reset(operationPointer);
	if (const HRESULT result = operationResult; FAILED(result))
		throw DmlProgramCompilerError("CreateOperator failed", result);

	std::unique_ptr<IDMLCompiledOperator, IUnknownReleaser> compiledOperation;
	IDMLCompiledOperator *compiledOperationPointer = nullptr;
	const HRESULT compiledOperationResult = impl_->dmlDevice_->CompileOperator(
		operation.get(), DML_EXECUTION_FLAG_NONE, IID_PPV_ARGS(&compiledOperationPointer));
	compiledOperation.reset(compiledOperationPointer);
	if (const HRESULT result = compiledOperationResult; FAILED(result))
		throw DmlProgramCompilerError("CompileOperator failed", result);
	impl_->operations_[impl_->nodeIndex_] = std::move(compiledOperation);
	impl_->dmlPersistentResources_[impl_->nodeIndex_] =
		makeDmlPersistentResource(impl_->d3dDevice_.get(), impl_->operations_[impl_->nodeIndex_].get());
	impl_->operatorsToReset_.push_back(impl_->operations_[impl_->nodeIndex_].get());
}

auto DmlProgramCompiler::operator()(const FusedNode &nodes) -> void
{
	std::visit(*this, nodes);
}

auto DmlProgramCompiler::operator()(const Onnx11Node &nodes) -> void
{
	std::visit(*this, nodes);
}

auto DmlProgramCompiler::operator()(const Onnx13Node &nodes) -> void
{
	std::visit(*this, nodes);
}

auto DmlProgramCompiler::operator()(const Onnx14Node &nodes) -> void
{
	std::visit(*this, nodes);
}

// MARK: - SDK Type Assertions

static_assert(static_cast<std::uint32_t>(dtUnknown) == DML_TENSOR_DATA_TYPE_UNKNOWN);
static_assert(static_cast<std::uint32_t>(dtFloat32) == DML_TENSOR_DATA_TYPE_FLOAT32);
static_assert(static_cast<std::uint32_t>(dtFloat16) == DML_TENSOR_DATA_TYPE_FLOAT16);
static_assert(static_cast<std::uint32_t>(dtUInt32) == DML_TENSOR_DATA_TYPE_UINT32);
static_assert(static_cast<std::uint32_t>(dtUInt16) == DML_TENSOR_DATA_TYPE_UINT16);
static_assert(static_cast<std::uint32_t>(dtUInt8) == DML_TENSOR_DATA_TYPE_UINT8);
static_assert(static_cast<std::uint32_t>(dtInt32) == DML_TENSOR_DATA_TYPE_INT32);
static_assert(static_cast<std::uint32_t>(dtInt16) == DML_TENSOR_DATA_TYPE_INT16);
static_assert(static_cast<std::uint32_t>(dtInt8) == DML_TENSOR_DATA_TYPE_INT8);
static_assert(static_cast<std::uint32_t>(dtFloat64) == DML_TENSOR_DATA_TYPE_FLOAT64);
static_assert(static_cast<std::uint32_t>(dtUInt64) == DML_TENSOR_DATA_TYPE_UINT64);
static_assert(static_cast<std::uint32_t>(dtInt64) == DML_TENSOR_DATA_TYPE_INT64);

} // namespace BaselineDml

namespace {

// MARK: - D3D12 Resource Helpers

constexpr auto bufferResourceDesc(std::uint64_t size, D3D12_RESOURCE_FLAGS flags) noexcept -> D3D12_RESOURCE_DESC
{
	return {
		.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
		.Width = std::max<std::uint64_t>(size, 4),
		.Height = 1,
		.DepthOrArraySize = 1,
		.MipLevels = 1,
		.SampleDesc = {1, 0},
		.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
		.Flags = flags,
	};
}

constexpr auto bufferTensorDesc(const BaselineDml::TensorSpec &spec) noexcept -> DML_BUFFER_TENSOR_DESC
{
	return {
		.DataType = static_cast<DML_TENSOR_DATA_TYPE>(BaselineDml::tensorDataType(spec)),
		.Flags = BaselineDml::isWeightTensor(spec) ? DML_TENSOR_FLAG_OWNED_BY_DML : DML_TENSOR_FLAG_NONE,
		.DimensionCount = BaselineDml::tensorRank(spec),
		.Sizes = BaselineDml::tensorSizes(spec).data(),
		.Strides = BaselineDml::tensorStrides(spec).data(),
		.TotalTensorSizeInBytes = BaselineDml::tensorSize(spec),
	};
}

} // namespace
