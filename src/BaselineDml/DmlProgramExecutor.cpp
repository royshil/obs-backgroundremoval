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

#include "DmlProgramExecutor.hpp"

#include <algorithm>
#include <cassert>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <TraceLoggingActivity.h>
#include <TraceLoggingProvider.h>

#include <cstddef>
#include <cstdint>
#include <span>

#include <d3d12.h>

#ifndef DML_TARGET_VERSION
#define DML_TARGET_VERSION 0x1000
#elif DML_TARGET_VERSION != 0x1000
#error "DML_TARGET_VERSION is not 0x1000"
#endif
#include <DirectML.h>

TRACELOGGING_DECLARE_PROVIDER(g_hProvider);

namespace {
void releaseCom(IUnknown *object) noexcept
{
	object->Release();
}
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

} // namespace

namespace BaselineDml {

struct DmlProgramExecutor::Impl final {
public:
	Impl(ID3D12Device *device, IDMLDevice *dmlDevice, const DmlProgramDefinition &definition,
	     const DmlProgram &program)
		: definition_(definition),
		  program_(program),
		  d3dDevice_(device),
		  dmlDevice_(dmlDevice)
	{
		if (!(device && dmlDevice))
			throw std::invalid_argument("Recording session requires D3D12 and DirectML devices");
		if (definition.nodes.empty() || definition.params.tensors.size() != definition.nodes.size() ||
		    program.operations.size() != definition.nodes.size() ||
		    program.tensorResources.size() != definition.nodes.size() ||
		    program.dmlPersistentResources.size() != definition.nodes.size())
			throw std::invalid_argument("Program resources do not match the node definition");
		for (const auto &resource : program.tensorResources)
			if (!resource)
				throw std::invalid_argument("Program node output resource is missing");
		std::unique_ptr<ID3D12Device, void (*)(IUnknown *)> parent{nullptr, releaseCom};
		ID3D12Device *parentPointer = nullptr;
		const auto parentResult = dmlDevice_->GetParentDevice(IID_PPV_ARGS(&parentPointer));
		parent.reset(parentPointer);
		if (FAILED(parentResult))
			throw std::runtime_error("Get DirectML parent device failed");
		if (parent.get() != device)
			throw std::invalid_argument("DirectML and D3D12 device mismatch");
		ID3D12Device *programDevice = nullptr;
		const auto programDeviceResult = program_.tensorResources[0]->GetDevice(IID_PPV_ARGS(&programDevice));
		parent.reset(programDevice);
		if (FAILED(programDeviceResult) || parent.get() != device)
			throw std::invalid_argument("Executor device differs from program device");

		IDMLCommandRecorder *recorderPointer = nullptr;
		const auto recorderResult = dmlDevice_->CreateCommandRecorder(IID_PPV_ARGS(&recorderPointer));
		recorder_.reset(recorderPointer);
		if (FAILED(recorderResult))
			throw std::runtime_error("CreateCommandRecorder failed");

		std::vector<NodeBindings> bindings(program_.operations.size());
		std::uint32_t descriptorCount = 0;
		temporaryResources_.reserve(program_.operations.size());
		executionTables_.reserve(program_.operations.size());
		for (std::size_t nodeIndex = 0; nodeIndex < program_.operations.size(); ++nodeIndex) {
			temporaryResources_.emplace_back(nullptr, releaseCom);
			executionTables_.emplace_back(nullptr, releaseCom);
			if (!program_.operations[nodeIndex])
				continue;
			auto &binding = bindings[nodeIndex];
			binding.operationProperties = program_.operations[nodeIndex]->GetBindingProperties();
			binding.descriptorOffset = descriptorCount;
			binding.descriptorCount =
				std::max<UINT>(1, binding.operationProperties.RequiredDescriptorCount);
			if (binding.descriptorCount > std::numeric_limits<std::uint32_t>::max() - descriptorCount)
				throw std::runtime_error("DirectML descriptor count overflow");
			descriptorCount += binding.descriptorCount;
			if (binding.operationProperties.TemporaryResourceSize) {
				D3D12_HEAP_PROPERTIES heap{};
				heap.Type = D3D12_HEAP_TYPE_DEFAULT;
				const auto description =
					bufferResourceDesc(binding.operationProperties.TemporaryResourceSize,
							   D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
				ID3D12Resource *resource = nullptr;
				const auto result = device->CreateCommittedResource(
					&heap, D3D12_HEAP_FLAG_NONE, &description,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&resource));
				temporaryResources_[nodeIndex].reset(resource);
				if (FAILED(result))
					throw std::runtime_error(
						std::string("Create execution temporary buffer failed (HRESULT ") +
						std::to_string(result) + ")");
			}
		}
		D3D12_DESCRIPTOR_HEAP_DESC heapDescription{};
		heapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDescription.NumDescriptors = std::max<UINT>(1, descriptorCount);
		heapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ID3D12DescriptorHeap *descriptorHeapPointer = nullptr;
		const auto descriptorHeapResult =
			device->CreateDescriptorHeap(&heapDescription, IID_PPV_ARGS(&descriptorHeapPointer));
		descriptorHeap_.reset(descriptorHeapPointer);
		if (FAILED(descriptorHeapResult))
			throw std::runtime_error("Create execution descriptor heap failed");
		createExecutionTables(bindings);
	}

	void recordGpuRun(ID3D12GraphicsCommandList *commands, std::span<const GpuInput> inputs,
			  ID3D12Resource *predicate, ID3D12Resource *output, std::uint64_t outputOffset) const noexcept
	{
		// Arguments are caller preconditions; recording never allocates, submits or throws.
#ifndef NDEBUG
		assertValidGpuRun(commands, inputs, predicate, output, outputOffset);
#endif
		commands->SetPredication(predicate, 0, D3D12_PREDICATION_OP_EQUAL_ZERO);
		recordInputCopies(commands, inputs);
		recordExecution(commands);
		recordOutputCopy(commands, output, outputOffset);
		for (const auto nodeIndex : definition_.params.inputTensors) {
			const D3D12_RESOURCE_BARRIER barrier{
				.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
				.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
				.Transition =
					{
						.pResource = resource(nodeIndex),
						.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
						.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
						.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST,
					},
			};
			commands->ResourceBarrier(1, &barrier);
		}
		commands->SetPredication(nullptr, 0, D3D12_PREDICATION_OP_EQUAL_ZERO);
	}

private:
#ifndef NDEBUG
	void assertValidGpuRun(ID3D12GraphicsCommandList *commands, std::span<const GpuInput> inputs,
			       ID3D12Resource *predicate, ID3D12Resource *output,
			       std::uint64_t outputOffset) const noexcept
	{
		assert(output != nullptr);
		assert(inputs.size() == definition_.params.inputTensors.size());
		assertValidCommands(commands);
		if (predicate)
			assertValidBuffer(predicate, 0, 8);
		assertValidBuffer(output, outputOffset, BaselineDml::outputInfo(definition_).dataBytes);
		D3D12_HEAP_PROPERTIES heap{};
		D3D12_HEAP_FLAGS flags{};
		const HRESULT result = output->GetHeapProperties(&heap, &flags);
		assert(SUCCEEDED(result));
		assert(heap.Type == D3D12_HEAP_TYPE_READBACK || heap.Type == D3D12_HEAP_TYPE_DEFAULT);
		for (std::size_t index = 0; index < inputs.size(); ++index) {
			const auto &tensor = definition_.params.tensors[definition_.params.inputTensors[index]];
			assert(inputs[index].dataBytes == tensorDataBytes(tensor));
			assertValidBuffer(inputs[index].resource, inputs[index].offset, tensorSize(tensor));
		}
	}

	void assertValidBuffer(ID3D12Resource *buffer, std::uint64_t offset, std::uint64_t bytes) const noexcept
	{
		assert(buffer != nullptr);
		const auto desc = buffer->GetDesc();
		std::unique_ptr<ID3D12Device, void (*)(IUnknown *)> owner{nullptr, releaseCom};
		ID3D12Device *ownerPointer = nullptr;
		const HRESULT result = buffer->GetDevice(IID_PPV_ARGS(&ownerPointer));
		owner.reset(ownerPointer);
		assert(SUCCEEDED(result));
		assert(owner.get() == d3dDevice_);
		assert(desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER);
		assert(offset <= desc.Width && bytes <= desc.Width - offset);
	}

	void assertValidCommands(ID3D12GraphicsCommandList *commands) const noexcept
	{
		assert(commands != nullptr);
		assert(commands->GetType() == D3D12_COMMAND_LIST_TYPE_DIRECT);
		std::unique_ptr<ID3D12Device, void (*)(IUnknown *)> owner{nullptr, releaseCom};
		ID3D12Device *ownerPointer = nullptr;
		const HRESULT result = commands->GetDevice(IID_PPV_ARGS(&ownerPointer));
		owner.reset(ownerPointer);
		assert(SUCCEEDED(result));
		assert(owner.get() == d3dDevice_);
	}
#endif

	void recordInputCopies(ID3D12GraphicsCommandList *commands, std::span<const GpuInput> inputs) const noexcept
	{
		for (std::size_t index = 0; index < inputs.size(); ++index) {
			const auto id = definition_.params.inputTensors[index];
			commands->CopyBufferRegion(resource(id), 0, inputs[index].resource, inputs[index].offset,
						   tensorSize(definition_.params.tensors[id]));
		}
		for (const auto nodeIndex : definition_.params.inputTensors) {
			const D3D12_RESOURCE_BARRIER barrier{
				.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
				.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
				.Transition =
					{
						.pResource = resource(nodeIndex),
						.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
						.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
						.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					},
			};
			commands->ResourceBarrier(1, &barrier);
		}
	}

	void recordExecution(ID3D12GraphicsCommandList *commands) const noexcept
	{
		ID3D12DescriptorHeap *heaps[]{descriptorHeap_.get()};
		commands->SetDescriptorHeaps(1, heaps);
		for (std::size_t index = 0; index < definition_.params.steps.size(); ++index) {
			const auto &description = definition_.params.steps[index];
			const auto barriers =
				std::span(uavBarriers_).subspan(description.barrierBegin, description.barrierCount);
			if (!barriers.empty())
				commands->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
			recordNode(commands, definition_.params.steps[index], index);
		}
	}

	void recordOutputCopy(ID3D12GraphicsCommandList *commands, ID3D12Resource *output,
			      std::uint64_t outputOffset) const noexcept
	{
		const auto &tensor = definition_.params.tensors[definition_.params.outputTensor];
		auto *const outputResource = resource(definition_.params.outputTensor);
		const D3D12_RESOURCE_BARRIER copyReady{
			.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
			.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
			.Transition =
				{
					.pResource = outputResource,
					.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
					.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE,
				},
		};
		commands->ResourceBarrier(1, &copyReady);
		commands->CopyBufferRegion(output, outputOffset, outputResource,
					   static_cast<std::uint64_t>(tensorByteOffset(tensor)),
					   BaselineDml::outputInfo(definition_).dataBytes);
		const D3D12_RESOURCE_BARRIER restored{
			.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
			.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
			.Transition =
				{
					.pResource = outputResource,
					.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
					.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE,
					.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				},
		};
		commands->ResourceBarrier(1, &restored);
	}

	auto resource(std::int64_t nodeIndex) const noexcept -> ID3D12Resource *
	{
		return program_.tensorResources[nodeIndex].get();
	}

	void recordNode(ID3D12GraphicsCommandList *commands, const StepDefinition &description,
			std::size_t nodeIndex) const noexcept
	{
		if (description.output == -1)
			return;
		if (description.shader == ProgramShader::None) {
			recorder_.get()->RecordDispatch(commands, program_.operations[nodeIndex].get(),
							executionTables_[nodeIndex].get());
			return;
		}
		const auto inputs = std::span(description.inputs).first(description.inputCount);
		commands->SetPipelineState(program_.pipelines[nodeIndex].get());
		commands->SetComputeRootSignature(program_.rootSignatures[nodeIndex].get());
		for (UINT index = 0; index < inputs.size(); ++index) {
			const auto id = inputs[index];
			const auto &tensor = definition_.params.tensors[id];
			commands->SetComputeRootUnorderedAccessView(
				index, resource(id)->GetGPUVirtualAddress() +
					       (static_cast<std::uint64_t>(tensorByteOffset(tensor))));
		}
		commands->SetComputeRootUnorderedAccessView(3, resource(description.output)->GetGPUVirtualAddress());
		if (description.shader == ProgramShader::MPLD10DepthwiseConv2dRelu)
			commands->Dispatch(16 * 9 * 128 / 64, 1, 1);
		else if (description.shader == ProgramShader::MPLD10BinarizeFloat32)
			commands->Dispatch((256 * 144 + 63) / 64, 1, 1);
	}

	struct NodeBindings final {
		DML_BINDING_PROPERTIES operationProperties{};
		std::uint32_t descriptorOffset = 0;
		std::uint32_t descriptorCount = 0;
	};

	auto makeTable(ID3D12DescriptorHeap *heap, IDMLDispatchable *dispatchable, std::uint32_t descriptorOffset,
		       UINT descriptorCount) -> std::unique_ptr<IDMLBindingTable, void (*)(IUnknown *)>
	{
		DML_BINDING_TABLE_DESC description{};
		description.Dispatchable = dispatchable;
		description.CPUDescriptorHandle = heap->GetCPUDescriptorHandleForHeapStart();
		description.CPUDescriptorHandle.ptr +=
			static_cast<SIZE_T>(descriptorOffset) *
			d3dDevice_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		description.GPUDescriptorHandle = heap->GetGPUDescriptorHandleForHeapStart();
		description.GPUDescriptorHandle.ptr +=
			static_cast<UINT64>(descriptorOffset) *
			d3dDevice_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		description.SizeInDescriptors = descriptorCount;
		IDMLBindingTable *tablePointer = nullptr;
		const auto result = dmlDevice_->CreateBindingTable(&description, IID_PPV_ARGS(&tablePointer));
		if (FAILED(result)) {
			if (tablePointer != nullptr) {
				tablePointer->Release();
			}
			throw std::runtime_error("CreateBindingTable failed (HRESULT " + std::to_string(result) +
						 ", device reason " +
						 std::to_string(dmlDevice_->GetDeviceRemovedReason()) + ")");
		}
		return std::unique_ptr<IDMLBindingTable, void (*)(IUnknown *)>{tablePointer, releaseCom};
	}

	void createExecutionTables(std::span<const NodeBindings> bindings)
	{
		// Resolve each model barrier once; step ranges remain those of the definition.
		uavBarriers_.reserve(definition_.params.uavBarriers.size());
		for (const auto id : definition_.params.uavBarriers) {
			D3D12_RESOURCE_BARRIER barrier{};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			barrier.UAV.pResource = resource(id);
			if (!barrier.UAV.pResource)
				throw std::runtime_error("UAV barrier target resource is unavailable");
			uavBarriers_.push_back(barrier);
		}

		for (std::size_t nodeIndex = 0; nodeIndex < program_.operations.size(); ++nodeIndex) {
			const auto &description = definition_.params.steps[nodeIndex];
			const auto &binding = bindings[nodeIndex];
			const auto inputs = std::span(description.inputs).first(description.inputCount);

			if (description.output == -1 || description.shader != ProgramShader::None)
				continue;
			try {
				executionTables_[nodeIndex] = makeTable(
					descriptorHeap_.get(), program_.operations[nodeIndex].get(),
					binding.descriptorOffset,
					std::max<UINT>(1, binding.operationProperties.RequiredDescriptorCount));
			} catch (const std::exception &error) {
				throw std::runtime_error("DirectML execution step " + std::to_string(nodeIndex) + ": " +
							 error.what());
			}
			std::vector<DML_BUFFER_BINDING> buffers(inputs.size());
			std::vector<DML_BINDING_DESC> bindings(inputs.size(), {DML_BINDING_TYPE_NONE, nullptr});
			for (std::size_t index = 0; index < inputs.size(); ++index) {
				const auto id = inputs[index];
				if (id != -1 && !isWeightTensor(definition_.params.tensors[id])) {
					buffers[index] = DML_BUFFER_BINDING{resource(id), 0,
									    tensorSize(definition_.params.tensors[id])};
					bindings[index] = DML_BINDING_DESC{DML_BINDING_TYPE_BUFFER, &buffers[index]};
				}
			}
			if (!bindings.empty()) {
				executionTables_[nodeIndex]->BindInputs(static_cast<UINT>(bindings.size()),
									bindings.data());
			}
			const auto outputBuffer =
				DML_BUFFER_BINDING{resource(description.output), 0,
						   tensorSize(definition_.params.tensors[description.output])};
			const auto outputBinding = DML_BINDING_DESC{DML_BINDING_TYPE_BUFFER, &outputBuffer};
			executionTables_[nodeIndex]->BindOutputs(1, &outputBinding);
			if (binding.operationProperties.TemporaryResourceSize) {
				const auto temporaryBuffer =
					DML_BUFFER_BINDING{temporaryResources_[nodeIndex].get(), 0,
							   binding.operationProperties.TemporaryResourceSize};
				const auto temporaryBinding =
					DML_BINDING_DESC{DML_BINDING_TYPE_BUFFER, &temporaryBuffer};
				executionTables_[nodeIndex]->BindTemporaryResource(&temporaryBinding);
			}
			if (const auto &persistentResource = program_.dmlPersistentResources[nodeIndex];
			    persistentResource) {
				const auto persistentBuffer =
					DML_BUFFER_BINDING{persistentResource.get(), 0,
							   binding.operationProperties.PersistentResourceSize};
				const auto persistentBinding =
					DML_BINDING_DESC{DML_BINDING_TYPE_BUFFER, &persistentBuffer};
				executionTables_[nodeIndex]->BindPersistentResource(&persistentBinding);
			}
		}
	}

	const DmlProgramDefinition definition_;
	const DmlProgram &program_;
	ID3D12Device *const d3dDevice_;
	IDMLDevice *const dmlDevice_;
	std::unique_ptr<IDMLCommandRecorder, void (*)(IUnknown *)> recorder_{nullptr, releaseCom};
	std::unique_ptr<ID3D12DescriptorHeap, void (*)(IUnknown *)> descriptorHeap_{nullptr, releaseCom};
	template<typename T> using NodeVector = std::vector<T>;
	NodeVector<std::unique_ptr<ID3D12Resource, void (*)(IUnknown *)>> temporaryResources_;
	NodeVector<std::unique_ptr<IDMLBindingTable, void (*)(IUnknown *)>> executionTables_;
	std::vector<D3D12_RESOURCE_BARRIER> uavBarriers_;
};

DmlProgramExecutor::DmlProgramExecutor(ID3D12Device *device, IDMLDevice *dmlDevice,
				       const DmlProgramDefinition &definition, const DmlProgram &program)
	: impl_(std::make_unique<Impl>(device, dmlDevice, definition, program))
{
}
DmlProgramExecutor::~DmlProgramExecutor() noexcept = default;

void DmlProgramExecutor::recordGpuRun(ID3D12GraphicsCommandList *commands, std::span<const GpuInput> inputs,
				      ID3D12Resource *predicate, ID3D12Resource *output,
				      std::uint64_t outputOffset) const noexcept
{
	impl_->recordGpuRun(commands, inputs, predicate, output, outputOffset);
}
} // namespace BaselineDml
