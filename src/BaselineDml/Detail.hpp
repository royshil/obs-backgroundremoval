// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "DmlProgramCompiler.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#if defined(DML_TARGET_VERSION) && DML_TARGET_VERSION != 0x1000
#error "DML_TARGET_VERSION is not 0x1000"
#else
#define DML_TARGET_VERSION 0x1000
#include <d3d12.h>
#include <d3dcompiler.h>
#include <DirectML.h>
#endif

namespace BaselineDml::Detail {

// Callers validate space for rounding; alignment is a nonzero constant at each call site.
constexpr auto alignTo(std::uint64_t value, std::uint64_t alignment) noexcept -> std::uint64_t
{
	return (value + alignment - 1) / alignment * alignment;
}

auto createBuffer(ID3D12Device *device, std::uint64_t size, D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES state,
		  D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE) -> ComOwner<ID3D12Resource>;
auto transition(ID3D12Resource *resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) noexcept
	-> D3D12_RESOURCE_BARRIER;

inline constexpr std::int64_t noTensor = -1;
using CompiledShader = Definition::ProgramShader;

// Internal tensors have the same type/shape/stride semantics as node tensors.
// Their IDs encode the owning node in the upper 32 bits and an operation-local ID below.
// Each operation fixes its local IDs at compile time in implementation code, not in the model definition.
// The model-assigned node index and fixed local ID determine the key without runtime ID allocation.
// An operation may access only its own internal tensors; they cannot form graph edges.
constexpr auto encodeInternalTensorId(std::uint32_t nodeIndex, std::uint32_t internalId) noexcept -> std::uint64_t
{
	return (std::uint64_t{nodeIndex} << 32) | std::uint64_t{internalId};
}

// Compile-time internal tensor specifications belong to the operation implementation, not model arrays.
// GPU ownership is kept separately in InternalTensors below.
struct InternalTensorDefinition final {
	std::uint64_t key;
	Definition::InputTensorSpec spec;
};

// Definitions belong to the operation implementation; only GPU ownership enters the program.
void allocateInternalTensors(ID3D12Device *device, std::span<const InternalTensorDefinition> definitions,
			     InternalTensors &tensors);

// DML opaque pointers own SDK-defined storage with no application-visible tensor layout.
// The SDK distinguishes temporary dispatch storage from persistent initialized storage.
// Neither resource is an internal tensor or uses an internal tensor ID.
struct DmlOpaquePointers final {
	ComOwner<ID3D12Resource> temporary;
	ComOwner<ID3D12Resource> persistent;
};

// Per-operation DirectML execution resources.
struct Step final {
	ComOwner<IDMLCompiledOperator> operation;
	ComOwner<ID3D12RootSignature> rootSignature;
	ComOwner<ID3D12PipelineState> pipeline;
	ComOwner<IDMLOperatorInitializer> initializer;
	ComOwner<IDMLBindingTable> initializationTable;
	ComOwner<IDMLBindingTable> executionTable;
	DmlOpaquePointers dmlOpaquePointers;
	std::vector<D3D12_RESOURCE_BARRIER> barriers;
};

struct TensorDescriptor final {
	DML_BUFFER_TENSOR_DESC buffer{};
	DML_TENSOR_DESC tensor{};
	explicit TensorDescriptor(const Definition::TensorSpec &value)
	{
		buffer.DataType = static_cast<DML_TENSOR_DATA_TYPE>(Definition::tensorDataType(value));
		buffer.Flags = Definition::isWeightTensor(value) ? DML_TENSOR_FLAG_OWNED_BY_DML : DML_TENSOR_FLAG_NONE;
		buffer.DimensionCount = Definition::tensorRank(value);
		buffer.Sizes = Definition::tensorSizes(value).data();
		buffer.Strides = Definition::tensorStrides(value).data();
		buffer.TotalTensorSizeInBytes = Definition::tensorSize(value);
		tensor = {DML_TENSOR_TYPE_BUFFER, &buffer};
	}
};

// Conv's rank-one bias becomes an SDK-only rank-four descriptor, not another model tensor.
struct ConvolutionBiasDescriptor final {
	std::array<UINT, 4> sizes;
	std::array<UINT, 4> strides;
	TensorDescriptor descriptor;

	explicit ConvolutionBiasDescriptor(const Definition::TensorSpec &bias)
		: sizes{1, Definition::tensorSizes(bias)[0], 1, 1},
		  strides{Definition::tensorSizes(bias)[0], 1, 1, 1},
		  descriptor(bias)
	{
		descriptor.buffer.DimensionCount = 4;
		descriptor.buffer.Sizes = sizes.data();
		descriptor.buffer.Strides = strides.data();
	}
};

} // namespace BaselineDml::Detail
