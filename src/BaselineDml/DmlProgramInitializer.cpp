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

#include "DmlProgramInitializer.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include <d3d12.h>

#ifndef DML_TARGET_VERSION
#define DML_TARGET_VERSION 0x1000
#elif DML_TARGET_VERSION != 0x1000
#error "DML_TARGET_VERSION is not 0x1000"
#endif
#include <DirectML.h>

namespace {

void releaseCom(IUnknown *object) noexcept
{
	object->Release();
}

template<typename T> using ComOwner = std::unique_ptr<T, void (*)(IUnknown *)>;

} // namespace

namespace BaselineDml {

struct DmlProgramInitializer::Impl final {
	Impl(ID3D12Device *device, IDMLDevice *dmlDevice, const DmlProgramDefinition &definition,
	     const DmlProgram &program);

	void recordInitialization(ID3D12GraphicsCommandList *commands) const noexcept;

	const DmlProgram &program_;
	const std::uint64_t weightBytes_;
	ComOwner<IDMLCommandRecorder> recorder_{nullptr, releaseCom};
	ComOwner<IDMLOperatorInitializer> initializer_{nullptr, releaseCom};
	ComOwner<ID3D12Resource> temporary_{nullptr, releaseCom};
	ComOwner<ID3D12DescriptorHeap> heap_{nullptr, releaseCom};
	// Declared last so it is released before the heap it writes.
	ComOwner<IDMLBindingTable> table_{nullptr, releaseCom};
};

DmlProgramInitializer::Impl::Impl(ID3D12Device *device, IDMLDevice *dmlDevice, const DmlProgramDefinition &definition,
				  const DmlProgram &program)
	: program_(program),
	  weightBytes_(definition.params.weightBytes)
{
	if (!(device && dmlDevice))
		throw std::invalid_argument("Initializer requires D3D12 and DirectML devices");
	if (definition.nodes.empty() || definition.params.tensors.size() != definition.nodes.size() ||
	    definition.params.steps.size() != definition.nodes.size() ||
	    program.operations.size() != definition.nodes.size() ||
	    program.dmlPersistentResources.size() != definition.nodes.size())
		throw std::invalid_argument("Program resources do not match the node definition");
	if (!program.weight || !program.weightUpload || weightBytes_ == 0)
		throw std::invalid_argument("Program weight buffers are missing");

	// Initialize the compiled operators in definition order.
	std::vector<IDMLCompiledOperator *> operators;
	operators.reserve(program.operations.size());
	for (const auto &operation : program.operations)
		if (operation)
			operators.push_back(operation.get());
	if (operators.empty())
		throw std::logic_error("No DirectML operators to initialize");
	if (operators.size() > std::numeric_limits<UINT>::max())
		throw std::invalid_argument("Too many operators for initialization");

	IDMLCommandRecorder *recorder = nullptr;
	const HRESULT recorderResult = dmlDevice->CreateCommandRecorder(IID_PPV_ARGS(&recorder));
	recorder_.reset(recorder);
	if (FAILED(recorderResult))
		throw DmlProgramInitializerError("Create initialization command recorder failed", recorderResult);

	IDMLOperatorInitializer *initializer = nullptr;
	const HRESULT initializerResult = dmlDevice->CreateOperatorInitializer(
		static_cast<UINT>(operators.size()), operators.data(), IID_PPV_ARGS(&initializer));
	initializer_.reset(initializer);
	if (FAILED(initializerResult))
		throw DmlProgramInitializerError("Create operator initializer failed", initializerResult);

	const auto properties = initializer_->GetBindingProperties();
	const auto descriptorCount = std::max<UINT>(1, properties.RequiredDescriptorCount);
	const D3D12_DESCRIPTOR_HEAP_DESC heapDescription{
		.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		.NumDescriptors = descriptorCount,
		.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
	};
	ID3D12DescriptorHeap *heap = nullptr;
	const HRESULT heapResult = device->CreateDescriptorHeap(&heapDescription, IID_PPV_ARGS(&heap));
	heap_.reset(heap);
	if (FAILED(heapResult))
		throw DmlProgramInitializerError("Create initialization heap failed", heapResult);

	const DML_BINDING_TABLE_DESC tableDescription{
		.Dispatchable = initializer_.get(),
		.CPUDescriptorHandle = heap_->GetCPUDescriptorHandleForHeapStart(),
		.GPUDescriptorHandle = heap_->GetGPUDescriptorHandleForHeapStart(),
		.SizeInDescriptors = descriptorCount,
	};
	IDMLBindingTable *table = nullptr;
	const HRESULT tableResult = dmlDevice->CreateBindingTable(&tableDescription, IID_PPV_ARGS(&table));
	table_.reset(table);
	if (FAILED(tableResult))
		throw DmlProgramInitializerError("Create initialization binding table failed", tableResult);

	// Bind weights as inputs and the operators' persistent resources as outputs. Binding writes the descriptors
	// immediately, so the descriptor storage only needs to live until the Bind calls return.
	std::vector<std::vector<DML_BUFFER_BINDING>> inputBuffers(program.operations.size());
	std::vector<DML_BUFFER_ARRAY_BINDING> inputArrays(program.operations.size());
	std::vector<DML_BUFFER_BINDING> persistentBuffers(program.operations.size());
	std::vector<DML_BINDING_DESC> inputs;
	std::vector<DML_BINDING_DESC> outputs;
	inputs.reserve(operators.size());
	outputs.reserve(operators.size());
	for (std::size_t nodeIndex = 0; nodeIndex < program.operations.size(); ++nodeIndex) {
		const auto &operation = program.operations[nodeIndex];
		if (!operation)
			continue;
		const auto &step = definition.params.steps[nodeIndex];
		auto &buffers = inputBuffers[nodeIndex];
		buffers.resize(step.inputCount);
		for (std::size_t input = 0; input < step.inputCount; ++input) {
			const auto tensorIndex = step.inputs[input];
			if (tensorIndex == -1 || !isWeightTensor(definition.params.tensors[tensorIndex]))
				continue;
			const auto &tensor = definition.params.tensors[tensorIndex];
			buffers[input] = DML_BUFFER_BINDING{program.weight.get(),
							    static_cast<std::uint64_t>(tensorByteOffset(tensor)),
							    tensorSize(tensor)};
		}
		inputArrays[nodeIndex] = {static_cast<UINT>(buffers.size()), buffers.data()};
		inputs.push_back({DML_BINDING_TYPE_BUFFER_ARRAY, &inputArrays[nodeIndex]});

		if (const auto &persistent = program.dmlPersistentResources[nodeIndex]; persistent) {
			persistentBuffers[nodeIndex] = DML_BUFFER_BINDING{
				persistent.get(), 0, operation->GetBindingProperties().PersistentResourceSize};
			outputs.push_back({DML_BINDING_TYPE_BUFFER, &persistentBuffers[nodeIndex]});
		} else {
			outputs.push_back({DML_BINDING_TYPE_NONE, nullptr});
		}
	}
	table_->BindInputs(static_cast<UINT>(inputs.size()), inputs.data());
	table_->BindOutputs(static_cast<UINT>(outputs.size()), outputs.data());

	if (properties.TemporaryResourceSize) {
		const D3D12_HEAP_PROPERTIES heapProperties{.Type = D3D12_HEAP_TYPE_DEFAULT};
		const D3D12_RESOURCE_DESC description{
			.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
			.Width = std::max<std::uint64_t>(properties.TemporaryResourceSize, 4),
			.Height = 1,
			.DepthOrArraySize = 1,
			.MipLevels = 1,
			.SampleDesc = {1, 0},
			.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
			.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		};
		ID3D12Resource *temporary = nullptr;
		const HRESULT temporaryResult = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE,
										&description,
										D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
										nullptr, IID_PPV_ARGS(&temporary));
		temporary_.reset(temporary);
		if (FAILED(temporaryResult))
			throw DmlProgramInitializerError("Create DML initialization temporary buffer failed",
							 temporaryResult);
		const DML_BUFFER_BINDING buffer{temporary_.get(), 0, properties.TemporaryResourceSize};
		const DML_BINDING_DESC binding{DML_BINDING_TYPE_BUFFER, &buffer};
		table_->BindTemporaryResource(&binding);
	}

	if (const HRESULT result = dmlDevice->GetDeviceRemovedReason(); FAILED(result))
		throw DmlProgramInitializerError("Bind batched initializer resources failed", result);
}

void DmlProgramInitializer::Impl::recordInitialization(ID3D12GraphicsCommandList *commands) const noexcept
{
	assert(commands);

	// Copy the prepared weight bytes into the GPU buffer before DirectML binds it as a UAV.
	commands->CopyBufferRegion(program_.weight.get(), 0, program_.weightUpload.get(), 0, weightBytes_);
	const D3D12_RESOURCE_BARRIER weightBarrier{
		.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
		.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
		.Transition =
			{
				.pResource = program_.weight.get(),
				.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
				.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
				.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			},
	};
	commands->ResourceBarrier(1, &weightBarrier);

	// Record initialization after the weight upload and order subsequent UAV accesses.
	ID3D12DescriptorHeap *heaps[]{heap_.get()};
	commands->SetDescriptorHeaps(1, heaps);
	recorder_->RecordDispatch(commands, initializer_.get(), table_.get());
	D3D12_RESOURCE_BARRIER uavBarrier{};
	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	commands->ResourceBarrier(1, &uavBarrier);
}

DmlProgramInitializer::DmlProgramInitializer(ID3D12Device *device, IDMLDevice *dmlDevice,
					     const DmlProgramDefinition &definition, const DmlProgram &program)
	: impl_(std::make_unique<Impl>(device, dmlDevice, definition, program))
{
}

DmlProgramInitializer::~DmlProgramInitializer() noexcept = default;

void DmlProgramInitializer::recordInitialization(ID3D12GraphicsCommandList *commands) const noexcept
{
	impl_->recordInitialization(commands);
}

} // namespace BaselineDml
