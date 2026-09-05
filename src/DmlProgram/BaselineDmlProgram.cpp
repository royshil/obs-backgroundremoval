// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define DML_TARGET_VERSION 0x1000

#include "BaselineDmlProgram.hpp"
#include "DmlProgram/BaselineDmlCompiler.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <d3d12.h>
#include <DirectML.h>
#include <wrl/client.h>

namespace BaselineDml {
namespace {

using Microsoft::WRL::ComPtr;
constexpr TensorId noTensor = -1;

void check(HRESULT result, const char *operation)
{
	if (FAILED(result)) {
		throw std::runtime_error(std::string(operation) + " failed (HRESULT " + std::to_string(result) + ")");
	}
}

auto createBuffer(ID3D12Device *device, std::uint64_t size, D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES state,
		  D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE) -> ComPtr<ID3D12Resource>
{
	D3D12_HEAP_PROPERTIES heap{};
	heap.Type = heapType;
	D3D12_RESOURCE_DESC description{};
	description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	description.Width = std::max<std::uint64_t>(size, 4);
	description.Height = 1;
	description.DepthOrArraySize = 1;
	description.MipLevels = 1;
	description.SampleDesc.Count = 1;
	description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	description.Flags = flags;
	ComPtr<ID3D12Resource> resource;
	check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description, state, nullptr,
					      IID_PPV_ARGS(&resource)),
	      "ID3D12Device::CreateCommittedResource");
	return resource;
}

auto transition(ID3D12Resource *resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
	-> D3D12_RESOURCE_BARRIER
{
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = resource;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = before;
	barrier.Transition.StateAfter = after;
	return barrier;
}

using Tensor = CompiledTensor;

struct Storage final {
	std::uint64_t size = 0;
	ComPtr<ID3D12Resource> resource;
};

struct HandleCloser final {
	void operator()(void *handle) const noexcept
	{
		if (handle) {
			CloseHandle(handle);
		}
	}
};

using UniqueHandle = std::unique_ptr<void, HandleCloser>;

struct Step final {
	std::vector<TensorId> inputs;
	TensorId output = noTensor;
	ComPtr<IDMLCompiledOperator> operation;
	ComPtr<IDMLOperatorInitializer> initializer;
	ComPtr<IDMLBindingTable> executionTable;
	ComPtr<ID3D12Resource> persistent;
	DML_BINDING_PROPERTIES operationProperties{};
	DML_BINDING_PROPERTIES initializerProperties{};
	std::uint32_t descriptorOffset = 0;
	std::uint32_t descriptorCount = 0;
};

} // namespace

namespace {

auto bufferBinding(ID3D12Resource *resource, std::uint64_t offset, std::uint64_t size) -> DML_BUFFER_BINDING
{
	return {resource, offset, size};
}

auto bindingDesc(const DML_BUFFER_BINDING *binding) -> DML_BINDING_DESC
{
	return {binding ? DML_BINDING_TYPE_BUFFER : DML_BINDING_TYPE_NONE, binding};
}

} // namespace

class BaselineDmlProgram::Impl final {
public:
	explicit Impl(const Graph &graph, std::span<const std::byte> weights)
	{
		check(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3dDevice_)),
		      "D3D12CreateDevice");
		check(DMLCreateDevice(d3dDevice_.Get(), DML_CREATE_DEVICE_FLAG_NONE, IID_PPV_ARGS(&dmlDevice_)),
		      "DMLCreateDevice");
		D3D12_COMMAND_QUEUE_DESC queueDescription{};
		queueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		check(d3dDevice_->CreateCommandQueue(&queueDescription, IID_PPV_ARGS(&queue_)), "CreateCommandQueue");
		check(d3dDevice_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator_)),
		      "CreateCommandAllocator");
		check(d3dDevice_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator_.Get(), nullptr,
						    IID_PPV_ARGS(&commandList_)),
		      "CreateCommandList");
		check(dmlDevice_->CreateCommandRecorder(IID_PPV_ARGS(&recorder_)), "CreateCommandRecorder");
		check(d3dDevice_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)), "CreateFence");
		fenceEvent_.reset(CreateEventW(nullptr, FALSE, FALSE, nullptr));
		if (!fenceEvent_) {
			throw std::runtime_error("CreateEventW failed");
		}

		BaselineDmlCompiler compiler(dmlDevice_.Get());
		auto compiled = compiler.compile(graph, weights);
		tensors_ = std::move(compiled.tensors);
		storages_.reserve(compiled.storages.size());
		for (const auto &storage : compiled.storages) {
			storages_.push_back({storage.size, {}});
		}
		steps_.reserve(compiled.steps.size());
		for (auto &compiledStep : compiled.steps) {
			Step step;
			step.inputs = std::move(compiledStep.inputs);
			step.output = compiledStep.output;
			step.operation = std::move(compiledStep.operation);
			steps_.push_back(std::move(step));
		}
		inputTensor_ = compiled.inputTensor;
		outputTensor_ = compiled.outputTensor;
		inputSize_ = tensors_.at(inputTensor_).size;
		outputSize_ = tensors_.at(outputTensor_).size;
		weightBytes_ = compiled.weightBytes;
		allocateResources(weights);
		initializeOperators();
		createExecutionTables();
	}

	~Impl() noexcept = default;

	void run(std::span<const float> input, std::span<float> output)
	{
		if (input.size_bytes() != inputSize_ || output.size_bytes() != outputSize_) {
			throw std::invalid_argument("Baseline DirectML tensor size mismatch");
		}
		void *mapped = nullptr;
		check(inputUpload_->Map(0, nullptr, &mapped), "Map input upload buffer");
		std::memcpy(mapped, input.data(), inputSize_);
		inputUpload_->Unmap(0, nullptr);

		resetCommands();
		auto *inputResource = resource(inputTensor_);
		commandList_->CopyBufferRegion(inputResource, 0, inputUpload_.Get(), 0, inputSize_);
		auto inputBarrier = transition(inputResource, D3D12_RESOURCE_STATE_COPY_DEST,
					       D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		commandList_->ResourceBarrier(1, &inputBarrier);
		ID3D12DescriptorHeap *heaps[] = {descriptorHeap_.Get()};
		commandList_->SetDescriptorHeaps(1, heaps);
		for (auto &step : steps_) {
			recorder_->RecordDispatch(commandList_.Get(), step.operation.Get(), step.executionTable.Get());
			D3D12_RESOURCE_BARRIER barrier{};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			commandList_->ResourceBarrier(1, &barrier);
		}
		auto *outputResource = resource(outputTensor_);
		auto outputBarrier = transition(outputResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
						D3D12_RESOURCE_STATE_COPY_SOURCE);
		commandList_->ResourceBarrier(1, &outputBarrier);
		commandList_->CopyBufferRegion(readback_.Get(), 0, outputResource, 0, outputSize_);
		std::array resetBarriers{
			transition(inputResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				   D3D12_RESOURCE_STATE_COPY_DEST),
			transition(outputResource, D3D12_RESOURCE_STATE_COPY_SOURCE,
				   D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
		};
		commandList_->ResourceBarrier(static_cast<UINT>(resetBarriers.size()), resetBarriers.data());
		executeAndWait();

		D3D12_RANGE readRange{0, outputSize_};
		check(readback_->Map(0, &readRange, &mapped), "Map output readback buffer");
		std::memcpy(output.data(), mapped, outputSize_);
		readback_->Unmap(0, nullptr);
	}

private:
	auto resource(TensorId id) -> ID3D12Resource * { return storages_.at(tensors_.at(id).storage).resource.Get(); }

	void allocateResources(std::span<const std::byte> weights)
	{
		for (std::size_t index = 0; index < storages_.size(); ++index) {
			auto &storage = storages_[index];
			const auto state = std::cmp_equal(index, tensors_.at(inputTensor_).storage)
						   ? D3D12_RESOURCE_STATE_COPY_DEST
						   : D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			storage.resource = createBuffer(d3dDevice_.Get(), storage.size, D3D12_HEAP_TYPE_DEFAULT, state,
							D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		}
		inputUpload_ = createBuffer(d3dDevice_.Get(), inputSize_, D3D12_HEAP_TYPE_UPLOAD,
					    D3D12_RESOURCE_STATE_GENERIC_READ);
		readback_ = createBuffer(d3dDevice_.Get(), outputSize_, D3D12_HEAP_TYPE_READBACK,
					 D3D12_RESOURCE_STATE_COPY_DEST);
		if (weightBytes_) {
			weightUpload_ = createBuffer(d3dDevice_.Get(), weightBytes_, D3D12_HEAP_TYPE_UPLOAD,
						     D3D12_RESOURCE_STATE_GENERIC_READ);
			weight_ = createBuffer(d3dDevice_.Get(), weightBytes_, D3D12_HEAP_TYPE_DEFAULT,
					       D3D12_RESOURCE_STATE_COPY_DEST,
					       D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
			void *mapped = nullptr;
			check(weightUpload_->Map(0, nullptr, &mapped), "Map generated weight upload buffer");
			for (const auto &tensor : tensors_) {
				if (tensor.weight) {
					std::memcpy(static_cast<std::byte *>(mapped) + tensor.weightOffset,
						    weights.data() + static_cast<std::size_t>(tensor.byteOffset),
						    static_cast<std::size_t>(tensor.weightCount) * sizeof(float));
				}
			}
			weightUpload_->Unmap(0, nullptr);
			commandList_->CopyBufferRegion(weight_.Get(), 0, weightUpload_.Get(), 0, weightBytes_);
			auto weightBarrier = transition(weight_.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
							D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			commandList_->ResourceBarrier(1, &weightBarrier);
		}

		std::uint64_t temporarySize = 0;
		std::uint32_t descriptorCount = 0;
		for (auto &step : steps_) {
			step.operationProperties = step.operation->GetBindingProperties();
			IDMLCompiledOperator *operations[] = {step.operation.Get()};
			check(dmlDevice_->CreateOperatorInitializer(1, operations, IID_PPV_ARGS(&step.initializer)),
			      "CreateOperatorInitializer");
			step.initializerProperties = step.initializer->GetBindingProperties();
			temporarySize = std::max({temporarySize, step.operationProperties.TemporaryResourceSize,
						  step.initializerProperties.TemporaryResourceSize});
			if (step.operationProperties.PersistentResourceSize) {
				step.persistent =
					createBuffer(d3dDevice_.Get(), step.operationProperties.PersistentResourceSize,
						     D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
						     D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
			}
			step.descriptorOffset = descriptorCount;
			step.descriptorCount =
				std::max<UINT>(1, std::max(step.operationProperties.RequiredDescriptorCount,
							   step.initializerProperties.RequiredDescriptorCount));
			descriptorCount += step.descriptorCount;
		}
		D3D12_DESCRIPTOR_HEAP_DESC heapDescription{};
		heapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDescription.NumDescriptors = descriptorCount;
		heapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		check(d3dDevice_->CreateDescriptorHeap(&heapDescription, IID_PPV_ARGS(&descriptorHeap_)),
		      "CreateDescriptorHeap");
		descriptorIncrement_ =
			d3dDevice_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		if (temporarySize) {
			temporary_ = createBuffer(d3dDevice_.Get(), temporarySize, D3D12_HEAP_TYPE_DEFAULT,
						  D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
						  D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		}
	}

	auto createTable(IDMLDispatchable *dispatchable, std::uint32_t descriptorOffset, UINT descriptorCount)
		-> ComPtr<IDMLBindingTable>
	{
		DML_BINDING_TABLE_DESC description{};
		description.Dispatchable = dispatchable;
		description.CPUDescriptorHandle = descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
		description.CPUDescriptorHandle.ptr += static_cast<SIZE_T>(descriptorOffset) * descriptorIncrement_;
		description.GPUDescriptorHandle = descriptorHeap_->GetGPUDescriptorHandleForHeapStart();
		description.GPUDescriptorHandle.ptr += static_cast<UINT64>(descriptorOffset) * descriptorIncrement_;
		description.SizeInDescriptors = descriptorCount;
		ComPtr<IDMLBindingTable> table;
		const auto result = dmlDevice_->CreateBindingTable(&description, IID_PPV_ARGS(&table));
		if (FAILED(result)) {
			throw std::runtime_error("CreateBindingTable failed (HRESULT " + std::to_string(result) +
						 ", device reason " +
						 std::to_string(d3dDevice_->GetDeviceRemovedReason()) + ")");
		}
		return table;
	}

	void bindTemporary(IDMLBindingTable *table, std::uint64_t size)
	{
		if (!size) {
			return;
		}
		const auto buffer = bufferBinding(temporary_.Get(), 0, size);
		const auto binding = bindingDesc(&buffer);
		table->BindTemporaryResource(&binding);
	}

	void initializeOperators()
	{
		ID3D12DescriptorHeap *heaps[] = {descriptorHeap_.Get()};
		commandList_->SetDescriptorHeaps(1, heaps);
		std::vector<ComPtr<IDMLBindingTable>> tables;
		tables.reserve(steps_.size());
		for (std::size_t stepIndex = 0; stepIndex < steps_.size(); ++stepIndex) {
			auto &step = steps_[stepIndex];
			const bool hasOwnedInput = std::ranges::any_of(step.inputs, [this](TensorId id) {
				return id != noTensor && tensors_.at(id).weight;
			});
			ComPtr<IDMLBindingTable> table;
			try {
				table = createTable(step.initializer.Get(), step.descriptorOffset,
						    std::max<UINT>(1,
								   step.initializerProperties.RequiredDescriptorCount));
			} catch (const std::exception &error) {
				throw std::runtime_error("DirectML initializer step " + std::to_string(stepIndex) +
							 ": " + error.what());
			}
			std::vector<DML_BUFFER_BINDING> buffers(step.inputs.size());
			for (std::size_t index = 0; index < step.inputs.size(); ++index) {
				const auto id = step.inputs[index];
				if (id != noTensor && tensors_.at(id).weight) {
					const auto &tensor = tensors_.at(id);
					buffers[index] = bufferBinding(weight_.Get(), tensor.weightOffset, tensor.size);
				}
			}
			DML_BUFFER_ARRAY_BINDING bufferArray{static_cast<UINT>(buffers.size()), buffers.data()};
			const DML_BINDING_DESC inputBinding =
				hasOwnedInput ? DML_BINDING_DESC{DML_BINDING_TYPE_BUFFER_ARRAY, &bufferArray}
					      : DML_BINDING_DESC{DML_BINDING_TYPE_NONE, nullptr};
			table->BindInputs(1, &inputBinding);
			bindTemporary(table.Get(), step.initializerProperties.TemporaryResourceSize);
			if (step.persistent) {
				const auto buffer = bufferBinding(step.persistent.Get(), 0,
								  step.operationProperties.PersistentResourceSize);
				const auto binding = bindingDesc(&buffer);
				table->BindOutputs(1, &binding);
			} else {
				const DML_BINDING_DESC binding{DML_BINDING_TYPE_NONE, nullptr};
				table->BindOutputs(1, &binding);
			}
			recorder_->RecordDispatch(commandList_.Get(), step.initializer.Get(), table.Get());
			tables.push_back(std::move(table));
			D3D12_RESOURCE_BARRIER barrier{};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			commandList_->ResourceBarrier(1, &barrier);
		}
		executeAndWait();
		for (auto &step : steps_) {
			step.initializer.Reset();
		}
		weightUpload_.Reset();
		weight_.Reset();
	}

	void createExecutionTables()
	{
		for (std::size_t stepIndex = 0; stepIndex < steps_.size(); ++stepIndex) {
			auto &step = steps_[stepIndex];
			try {
				step.executionTable = createTable(
					step.operation.Get(), step.descriptorOffset,
					std::max<UINT>(1, step.operationProperties.RequiredDescriptorCount));
			} catch (const std::exception &error) {
				throw std::runtime_error("DirectML execution step " + std::to_string(stepIndex) + ": " +
							 error.what());
			}
			std::vector<DML_BUFFER_BINDING> buffers(step.inputs.size());
			std::vector<DML_BINDING_DESC> bindings(step.inputs.size(), {DML_BINDING_TYPE_NONE, nullptr});
			for (std::size_t index = 0; index < step.inputs.size(); ++index) {
				const auto id = step.inputs[index];
				if (id != noTensor && !tensors_.at(id).weight) {
					buffers[index] = bufferBinding(resource(id), 0, tensors_.at(id).size);
					bindings[index] = bindingDesc(&buffers[index]);
				}
			}
			if (!bindings.empty()) {
				step.executionTable->BindInputs(static_cast<UINT>(bindings.size()), bindings.data());
			}
			const auto outputBuffer =
				bufferBinding(resource(step.output), 0, tensors_.at(step.output).size);
			const auto outputBinding = bindingDesc(&outputBuffer);
			step.executionTable->BindOutputs(1, &outputBinding);
			bindTemporary(step.executionTable.Get(), step.operationProperties.TemporaryResourceSize);
			if (step.persistent) {
				const auto persistentBuffer = bufferBinding(
					step.persistent.Get(), 0, step.operationProperties.PersistentResourceSize);
				const auto persistentBinding = bindingDesc(&persistentBuffer);
				step.executionTable->BindPersistentResource(&persistentBinding);
			}
		}
	}

	void resetCommands()
	{
		check(allocator_->Reset(), "CommandAllocator::Reset");
		check(commandList_->Reset(allocator_.Get(), nullptr), "CommandList::Reset");
	}

	void executeAndWait()
	{
		check(commandList_->Close(), "CommandList::Close");
		ID3D12CommandList *lists[] = {commandList_.Get()};
		queue_->ExecuteCommandLists(1, lists);
		const std::uint64_t value = ++fenceValue_;
		check(queue_->Signal(fence_.Get(), value), "CommandQueue::Signal");
		if (fence_->GetCompletedValue() < value) {
			check(fence_->SetEventOnCompletion(value, fenceEvent_.get()), "Fence::SetEventOnCompletion");
			WaitForSingleObject(fenceEvent_.get(), INFINITE);
		}
	}

	std::vector<Tensor> tensors_;
	std::vector<Storage> storages_;
	std::vector<Step> steps_;
	TensorId inputTensor_ = noTensor;
	TensorId outputTensor_ = noTensor;
	std::uint64_t weightBytes_ = 0;
	std::uint64_t inputSize_ = 0;
	std::uint64_t outputSize_ = 0;
	ComPtr<ID3D12Device> d3dDevice_;
	ComPtr<IDMLDevice> dmlDevice_;
	ComPtr<ID3D12CommandQueue> queue_;
	ComPtr<ID3D12CommandAllocator> allocator_;
	ComPtr<ID3D12GraphicsCommandList> commandList_;
	ComPtr<IDMLCommandRecorder> recorder_;
	ComPtr<ID3D12DescriptorHeap> descriptorHeap_;
	std::uint32_t descriptorIncrement_ = 0;
	ComPtr<ID3D12Resource> inputUpload_, readback_, weightUpload_, weight_, temporary_;
	ComPtr<ID3D12Fence> fence_;
	UniqueHandle fenceEvent_;
	std::uint64_t fenceValue_ = 0;
};

BaselineDmlProgram::BaselineDmlProgram(const Graph &graph, std::span<const std::byte> weights)
	: impl_(std::make_unique<Impl>(graph, weights))
{
}
BaselineDmlProgram::~BaselineDmlProgram() noexcept = default;
void BaselineDmlProgram::run(std::span<const float> input, std::span<float> output)
{
	impl_->run(input, output);
}

} // namespace BaselineDml
