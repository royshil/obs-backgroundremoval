// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef NOMINMAX
#define NOMINMAX
#endif

#define DML_TARGET_VERSION 0x1000

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <TraceLoggingProvider.h>
#include <TraceLoggingActivity.h>

TRACELOGGING_DECLARE_PROVIDER(g_hProvider);
#include "BaselineDmlProgramRuntime.hpp"
#include "DmlProgram/BaselineDmlCompiler.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <d3d12.h>
#include <d3dcompiler.h>
#include <DirectML.h>
#include <wrl/client.h>

namespace BaselineDml {
namespace {

// Keep SDK numeric compatibility checked at the runtime boundary.
static_assert(static_cast<std::uint32_t>(ProgramDefinition::dtUnknown) ==
	      DML_TENSOR_DATA_TYPE_UNKNOWN);
static_assert(static_cast<std::uint32_t>(ProgramDefinition::dtFloat32) ==
	      DML_TENSOR_DATA_TYPE_FLOAT32);
static_assert(static_cast<std::uint32_t>(ProgramDefinition::dtFloat16) ==
	      DML_TENSOR_DATA_TYPE_FLOAT16);
static_assert(static_cast<std::uint32_t>(ProgramDefinition::dtUInt32) == DML_TENSOR_DATA_TYPE_UINT32);
static_assert(static_cast<std::uint32_t>(ProgramDefinition::dtUInt16) == DML_TENSOR_DATA_TYPE_UINT16);
static_assert(static_cast<std::uint32_t>(ProgramDefinition::dtUInt8) == DML_TENSOR_DATA_TYPE_UINT8);
static_assert(static_cast<std::uint32_t>(ProgramDefinition::dtInt32) == DML_TENSOR_DATA_TYPE_INT32);
static_assert(static_cast<std::uint32_t>(ProgramDefinition::dtInt16) == DML_TENSOR_DATA_TYPE_INT16);
static_assert(static_cast<std::uint32_t>(ProgramDefinition::dtInt8) == DML_TENSOR_DATA_TYPE_INT8);
static_assert(static_cast<std::uint32_t>(ProgramDefinition::dtFloat64) ==
	      DML_TENSOR_DATA_TYPE_FLOAT64);
static_assert(static_cast<std::uint32_t>(ProgramDefinition::dtUInt64) == DML_TENSOR_DATA_TYPE_UINT64);
static_assert(static_cast<std::uint32_t>(ProgramDefinition::dtInt64) == DML_TENSOR_DATA_TYPE_INT64);

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
	std::vector<TensorId> barriersBefore;
	TensorId output = noTensor;
	ComPtr<IDMLCompiledOperator> operation;
	CompiledShader shader = CompiledShader::None;
	ComPtr<ID3D12RootSignature> rootSignature;
	ComPtr<ID3D12PipelineState> pipeline;
	ComPtr<IDMLOperatorInitializer> initializer;
	ComPtr<IDMLBindingTable> executionTable;
	ComPtr<ID3D12Resource> persistent;
	ComPtr<ID3D12Resource> temporary;
	std::vector<D3D12_RESOURCE_BARRIER> barriers;
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

class BaselineDmlProgramRuntime::Impl final {
public:
	explicit Impl(const Graph &graph, std::span<const std::byte> weights)
	{
		TraceLoggingThreadActivity<g_hProvider, 0x1> initialization;
		TraceLoggingWriteStart(initialization, "Runtime.Initialize");
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
		auto compiled = [&] {
			TraceLoggingThreadActivity<g_hProvider, 0x1> scope;
			TraceLoggingWriteStart(scope, "Runtime.Compile");
			return compiler.compile(graph, weights);
		}();
		tensors_ = std::move(compiled.tensors);
		storages_.reserve(compiled.storages.size());
		for (const auto &storage : compiled.storages) {
			storages_.push_back({storage.size, {}});
		}
		steps_.reserve(compiled.steps.size());
		for (auto &compiledStep : compiled.steps) {
			Step step;
			step.inputs = std::move(compiledStep.inputs);
			step.barriersBefore = std::move(compiledStep.barriersBefore);
			step.output = compiledStep.output;
			step.operation = std::move(compiledStep.operation);
			step.shader = compiledStep.shader;
			steps_.push_back(std::move(step));
		}
		inputTensors_ = std::move(compiled.inputTensors);
		outputTensor_ = compiled.outputTensor;

		outputSize_ = tensors_.at(outputTensor_).dataBytes;
		weightBytes_ = compiled.weightBytes;
		allocateResources(weights);
		initializeOperators();
		createExecutionTables();
	}

	~Impl() noexcept = default;

	void run(std::span<const std::span<const std::byte>> inputs, std::span<std::byte> output)
	{
		TraceLoggingFunction(g_hProvider, TraceLoggingKeyword(0x1));
		if (inputs.size() != inputTensors_.size())
			throw std::invalid_argument("Baseline DirectML input count mismatch");
		if (output.size() != outputSize_)
			throw std::invalid_argument("Baseline DirectML output byte size mismatch");
		for (std::size_t index = 0; index < inputs.size(); ++index) {
			if (inputs[index].size() != tensors_.at(inputTensors_[index]).dataBytes)
				throw std::invalid_argument("Baseline DirectML input " + std::to_string(index) +
							    " byte size mismatch");
		}
		// GPU-only callers never allocate CPU input upload buffers.
		if (inputUploads_.empty()) {
			std::vector<ComPtr<ID3D12Resource>> uploads;
			for (const auto id : inputTensors_)
				uploads.push_back(createBuffer(d3dDevice_.Get(), tensors_.at(id).size,
							       D3D12_HEAP_TYPE_UPLOAD,
							       D3D12_RESOURCE_STATE_GENERIC_READ));
			inputUploads_ = std::move(uploads);
		}
		// Validate every span before mapping or submitting any GPU work.
		TraceLoggingThreadActivity<g_hProvider, 0x1> upload;
		TraceLoggingWriteStart(upload, "Runtime.UploadCpuCopy");
		for (std::size_t index = 0; index < inputs.size(); ++index) {
			const auto &tensor = tensors_.at(inputTensors_[index]);
			void *mapped = nullptr;
			const D3D12_RANGE noRead{0, 0};
			check(inputUploads_[index]->Map(0, &noRead, &mapped), "Map input upload buffer");
			std::memset(mapped, 0, static_cast<std::size_t>(tensor.size));
			std::memcpy(mapped, inputs[index].data(), inputs[index].size());
			inputUploads_[index]->Unmap(0, nullptr);
		}
		TraceLoggingWriteStop(upload, "Runtime.UploadCpuCopy");
		TraceLoggingThreadActivity<g_hProvider, 0x1> recording;
		TraceLoggingWriteStart(recording, "Runtime.RecordCommands");
		resetCommands();
		for (std::size_t index = 0; index < inputs.size(); ++index) {
			const auto id = inputTensors_[index];
			auto *inputResource = resource(id);
			commandList_->CopyBufferRegion(inputResource, 0, inputUploads_[index].Get(), 0,
						       tensors_.at(id).size);
			const auto barrier = transition(inputResource, D3D12_RESOURCE_STATE_COPY_DEST,
							D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			commandList_->ResourceBarrier(1, &barrier);
		}
		ID3D12DescriptorHeap *heaps[] = {descriptorHeap_.Get()};
		commandList_->SetDescriptorHeaps(1, heaps);
		for (auto &step : steps_) {
			if (!step.barriers.empty())
				commandList_->ResourceBarrier(static_cast<UINT>(step.barriers.size()),
							      step.barriers.data());
			recordStep(commandList_.Get(), step);
#ifdef DML_MODEL_DIAGNOSTICS
            if (!diagnosticRoundedSteps_.empty()) roundDiagnosticStep(step, &step - steps_.data());
#endif
		}
#ifdef DML_MODEL_DIAGNOSTICS
        if (!diagnosticRoundedSteps_.empty()) recordDiagnosticCopies();
#endif
		auto *outputResource = resource(outputTensor_);
		const auto &outputTensor = tensors_.at(outputTensor_);
		auto outputBarrier = transition(outputResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
						D3D12_RESOURCE_STATE_COPY_SOURCE);
		commandList_->ResourceBarrier(1, &outputBarrier);
		commandList_->CopyBufferRegion(readback_.Get(), 0, outputResource,
					       outputTensor.weight ? outputTensor.weightOffset : 0, outputSize_);
		outputBarrier = transition(outputResource, D3D12_RESOURCE_STATE_COPY_SOURCE,
					   D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		commandList_->ResourceBarrier(1, &outputBarrier);
		// Restore separately so a pass-through output may alias a runtime input.
		for (const auto id : inputTensors_) {
			const auto barrier = transition(resource(id), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
							D3D12_RESOURCE_STATE_COPY_DEST);
			commandList_->ResourceBarrier(1, &barrier);
		}
		TraceLoggingWriteStop(recording, "Runtime.RecordCommands");
		executeAndWait();
#ifdef DML_MODEL_DIAGNOSTICS
        if (!diagnosticRoundedSteps_.empty()) readDiagnosticCopies();
#endif
		TraceLoggingThreadActivity<g_hProvider, 0x1> readback;
		TraceLoggingWriteStart(readback, "Runtime.ReadbackCpuCopy");
		void *mapped = nullptr;
		const D3D12_RANGE readRange{0, static_cast<SIZE_T>(outputSize_)};
		check(readback_->Map(0, &readRange, &mapped), "Map output readback buffer");
		std::memcpy(output.data(), mapped, output.size());
		const D3D12_RANGE noWrite{0, 0};
		readback_->Unmap(0, &noWrite);
	}

	auto device() const noexcept -> ID3D12Device * { return d3dDevice_.Get(); }
	auto queue() const noexcept -> ID3D12CommandQueue * { return queue_.Get(); }
	auto inputCount() const noexcept -> std::size_t { return inputTensors_.size(); }
	auto inputInfo(std::size_t index) const -> RuntimeTensorInfo
	{
		const auto &tensor = tensors_.at(inputTensors_.at(index));
		return {tensor.dataType, tensor.dataBytes};
	}
	auto outputInfo() const noexcept -> RuntimeTensorInfo
	{
		const auto &tensor = tensors_[outputTensor_];
		return {tensor.dataType, tensor.dataBytes};
	}

	void recordGpuRun(ID3D12GraphicsCommandList *commands, std::span<const GpuInput> inputs,
			  ID3D12Resource *predicate, ID3D12Resource *output, std::uint64_t outputOffset)
	{
		if (!commands || !output || inputs.size() != inputTensors_.size())
			throw std::invalid_argument("Invalid DirectML GPU submission");
		auto validateBuffer = [&](ID3D12Resource *buffer, std::uint64_t offset, std::uint64_t bytes) {
			if (!buffer)
				throw std::invalid_argument("Null DirectML GPU buffer");
			const auto desc = buffer->GetDesc();
			ComPtr<ID3D12Device> owner;
			check(buffer->GetDevice(IID_PPV_ARGS(&owner)), "Get GPU buffer device");
			if (owner.Get() != d3dDevice_.Get() || desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER ||
			    offset > desc.Width || bytes > desc.Width - offset)
				throw std::invalid_argument("DirectML GPU buffer device or byte range mismatch");
		};
		ComPtr<ID3D12Device> commandDevice;
		check(commands->GetDevice(IID_PPV_ARGS(&commandDevice)), "Get command list device");
		if (commandDevice.Get() != d3dDevice_.Get())
			throw std::invalid_argument("DirectML command list device mismatch");
		if (predicate)
			validateBuffer(predicate, 0, 8);
		validateBuffer(output, outputOffset, outputSize_);
		D3D12_HEAP_PROPERTIES heap{};
		D3D12_HEAP_FLAGS heapFlags{};
		check(output->GetHeapProperties(&heap, &heapFlags), "Get GPU output heap");
		if (heap.Type != D3D12_HEAP_TYPE_READBACK && heap.Type != D3D12_HEAP_TYPE_DEFAULT)
			throw std::invalid_argument("GPU output must be a default or readback buffer");
		for (std::size_t index = 0; index < inputs.size(); ++index) {
			const auto &tensor = tensors_.at(inputTensors_[index]);
			if (inputs[index].dataBytes != tensor.dataBytes)
				throw std::invalid_argument("DirectML GPU input byte size mismatch");
			validateBuffer(inputs[index].resource, inputs[index].offset, tensor.size);
		}
		commands->SetPredication(predicate, 0, D3D12_PREDICATION_OP_EQUAL_ZERO);
		for (std::size_t index = 0; index < inputs.size(); ++index) {
			const auto id = inputTensors_[index];
			commands->CopyBufferRegion(resource(id), 0, inputs[index].resource, inputs[index].offset,
						   tensors_.at(id).size);
			const auto barrier = transition(resource(id), D3D12_RESOURCE_STATE_COPY_DEST,
							D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			commands->ResourceBarrier(1, &barrier);
		}
		ID3D12DescriptorHeap *heaps[]{descriptorHeap_.Get()};
		commands->SetDescriptorHeaps(1, heaps);
		for (auto &step : steps_) {
			if (!step.barriers.empty())
				commands->ResourceBarrier(static_cast<UINT>(step.barriers.size()),
							  step.barriers.data());
			recordStep(commands, step);
		}
		auto *outputResource = resource(outputTensor_);
		const auto &tensor = tensors_.at(outputTensor_);
		auto barrier = transition(outputResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					  D3D12_RESOURCE_STATE_COPY_SOURCE);
		commands->ResourceBarrier(1, &barrier);
		commands->CopyBufferRegion(output, outputOffset, outputResource,
					   tensor.weight ? tensor.weightOffset : 0, outputSize_);
		barrier = transition(outputResource, D3D12_RESOURCE_STATE_COPY_SOURCE,
				     D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		commands->ResourceBarrier(1, &barrier);
		for (const auto id : inputTensors_) {
			barrier = transition(resource(id), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					     D3D12_RESOURCE_STATE_COPY_DEST);
			commands->ResourceBarrier(1, &barrier);
		}
		commands->SetPredication(nullptr, 0, D3D12_PREDICATION_OP_EQUAL_ZERO);
	}

#ifdef DML_MODEL_DIAGNOSTICS
#include "StorageDiagnostics.inl"
#endif
private:
	auto resource(TensorId id) -> ID3D12Resource *
	{
		const auto &tensor = tensors_.at(id);
		return tensor.weight ? weight_.Get() : storages_.at(tensor.storage).resource.Get();
	}

	void allocateResources(std::span<const std::byte> weights)
	{
		for (std::size_t index = 0; index < storages_.size(); ++index) {
			auto &storage = storages_[index];
			const auto state =
				std::ranges::any_of(inputTensors_,
						    [&](TensorId id) {
							    return std::cmp_equal(index, tensors_.at(id).storage);
						    })
					? D3D12_RESOURCE_STATE_COPY_DEST
					: D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			storage.resource = createBuffer(d3dDevice_.Get(), storage.size, D3D12_HEAP_TYPE_DEFAULT, state,
							D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		}
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
			std::memset(mapped, 0, static_cast<std::size_t>(weightBytes_));
			for (const auto &tensor : tensors_) {
				if (tensor.weight) {
					std::memcpy(static_cast<std::byte *>(mapped) + tensor.weightOffset,
						    weights.data() + static_cast<std::size_t>(tensor.byteOffset),
						    static_cast<std::size_t>(tensor.dataBytes));
				}
			}
			weightUpload_->Unmap(0, nullptr);
			commandList_->CopyBufferRegion(weight_.Get(), 0, weightUpload_.Get(), 0, weightBytes_);
			auto weightBarrier = transition(weight_.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
							D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			commandList_->ResourceBarrier(1, &weightBarrier);
		}

		std::uint32_t descriptorCount = 0;
		for (auto &step : steps_) {
			if (step.output == noTensor)
				continue;
			if (step.shader != CompiledShader::None) {
				createMPLDDepthwiseConv2dReluPipeline(step);
				continue;
			}
			step.operationProperties = step.operation->GetBindingProperties();
			IDMLCompiledOperator *operations[] = {step.operation.Get()};
			check(dmlDevice_->CreateOperatorInitializer(1, operations, IID_PPV_ARGS(&step.initializer)),
			      "CreateOperatorInitializer");
			step.initializerProperties = step.initializer->GetBindingProperties();
			const auto temporarySize = std::max(step.operationProperties.TemporaryResourceSize,
							    step.initializerProperties.TemporaryResourceSize);
			if (temporarySize)
				step.temporary = createBuffer(d3dDevice_.Get(), temporarySize, D3D12_HEAP_TYPE_DEFAULT,
							      D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
							      D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
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
			if (step.descriptorCount > std::numeric_limits<std::uint32_t>::max() - descriptorCount)
				throw std::runtime_error("DirectML descriptor count overflow");
			descriptorCount += step.descriptorCount;
		}
		D3D12_DESCRIPTOR_HEAP_DESC heapDescription{};
		heapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDescription.NumDescriptors = std::max<UINT>(1, descriptorCount);
		heapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		check(d3dDevice_->CreateDescriptorHeap(&heapDescription, IID_PPV_ARGS(&descriptorHeap_)),
		      "CreateDescriptorHeap");
		descriptorIncrement_ =
			d3dDevice_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
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
						 std::to_string(dmlDevice_->GetDeviceRemovedReason()) + ")");
		}
		return table;
	}

	void bindTemporary(IDMLBindingTable *table, ID3D12Resource *resource, std::uint64_t size)
	{
		if (!size) {
			return;
		}
		const auto buffer = bufferBinding(resource, 0, size);
		const auto binding = bindingDesc(&buffer);
		table->BindTemporaryResource(&binding);
	}

	void initializeOperators()
	{
		TraceLoggingThreadActivity<g_hProvider, 0x1> scope;
		TraceLoggingWriteStart(scope, "Runtime.InitializeOperators");
		ID3D12DescriptorHeap *heaps[] = {descriptorHeap_.Get()};
		commandList_->SetDescriptorHeaps(1, heaps);
		std::vector<ComPtr<IDMLBindingTable>> tables;
		tables.reserve(steps_.size());
		for (std::size_t stepIndex = 0; stepIndex < steps_.size(); ++stepIndex) {
			auto &step = steps_[stepIndex];
			if (step.output == noTensor || step.shader != CompiledShader::None)
				continue;
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
			const DML_BINDING_DESC inputBinding{DML_BINDING_TYPE_BUFFER_ARRAY, &bufferArray};
			table->BindInputs(1, &inputBinding);
			check(dmlDevice_->GetDeviceRemovedReason(),
			      ("initializer step " + std::to_string(stepIndex) + " BindInputs").c_str());
			bindTemporary(table.Get(), step.temporary.Get(),
				      step.initializerProperties.TemporaryResourceSize);
			check(dmlDevice_->GetDeviceRemovedReason(), "initializer BindTemporaryResource");
			if (step.persistent) {
				const auto buffer = bufferBinding(step.persistent.Get(), 0,
								  step.operationProperties.PersistentResourceSize);
				const auto binding = bindingDesc(&buffer);
				table->BindOutputs(1, &binding);
			} else {
				const DML_BINDING_DESC binding{DML_BINDING_TYPE_NONE, nullptr};
				table->BindOutputs(1, &binding);
			}
			check(dmlDevice_->GetDeviceRemovedReason(), "initializer BindOutputs");
			recorder_->RecordDispatch(commandList_.Get(), step.initializer.Get(), table.Get());
			check(dmlDevice_->GetDeviceRemovedReason(), "initializer RecordDispatch");
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
		const bool needsSourceWeights = std::ranges::any_of(steps_, [&](const Step &step) {
			return step.shader != CompiledShader::None ||
			       std::ranges::any_of(step.barriersBefore,
						   [&](TensorId id) { return tensors_.at(id).weight; });
		});
		if (!tensors_.at(outputTensor_).weight && !needsSourceWeights)
			weight_.Reset();
	}

	void createMPLDDepthwiseConv2dReluPipeline(Step &step)
	{
		static constexpr char shader[] = R"HLSL(
#define W 128
#define H 72
#define C 16
#define OW 128
#define OH 72
#define PX 1
#define PY 1
#define STRIP 8
#define THREADS 64

RWByteAddressBuffer input:register(u0);RWByteAddressBuffer weights:register(u1);RWByteAddressBuffer bias:register(u2);RWByteAddressBuffer output:register(u3);

// Independent vertical strips: adjacent lanes cover adjacent x coordinates.
// Rolling rows stay thread-local; every raw load uses a clamped address.
float3 loadRow(uint channel, uint ox, int y) {
 float3 row;
 [unroll]for(uint kx=0;kx<3;++kx){
  int x=int(ox)+int(kx)-PX;
  int cx=clamp(x,0,W-1),cy=clamp(y,0,H-1);
  float v=asfloat(input.Load(4*((channel*H+cy)*W+cx)));
  row[kx]=(x>=0&&x<W&&y>=0&&y<H)?v:0;
 }
 return row;
}
[numthreads(THREADS,1,1)]void main(uint3 id:SV_DispatchThreadID){
 const uint rows=(OH+STRIP-1)/STRIP;
 uint i=id.x;if(i>=C*rows*OW)return;
 uint channel=i/(rows*OW),ox=i%OW,oy=((i/OW)%rows)*STRIP;
 float3 w0,w1,w2;
 [unroll]for(uint k=0;k<3;++k){
  w0[k]=asfloat(weights.Load(4*(channel*9+k)));
  w1[k]=asfloat(weights.Load(4*(channel*9+3+k)));
  w2[k]=asfloat(weights.Load(4*(channel*9+6+k)));
 }
 float b=asfloat(bias.Load(4*channel));
 float3 r0=loadRow(channel,ox,int(oy)-PY);
 float3 r1=loadRow(channel,ox,int(oy)+1-PY);
 [unroll]for(uint n=0;n<STRIP;++n){
  float3 r2=loadRow(channel,ox,int(oy+n)+2-PY);
  precise float sum=0;
  [unroll]for(uint k0=0;k0<3;++k0)sum=sum+r0[k0]*w0[k0];
  [unroll]for(uint k1=0;k1<3;++k1)sum=sum+r1[k1]*w1[k1];
  [unroll]for(uint k2=0;k2<3;++k2)sum=sum+r2[k2]*w2[k2];
  if(oy+n<OH)output.Store(4*((channel*OH+oy+n)*OW+ox),asuint(max(sum+b,0)));
  r0=r1;r1=r2;
 }
}

)HLSL";
		ComPtr<ID3DBlob> bytecode, errors;
		check(D3DCompile(shader, sizeof(shader) - 1, "MPLDDepthwiseConv2dRelu", nullptr, nullptr, "main", "cs_5_0",
			D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_IEEE_STRICTNESS, 0, &bytecode, &errors), "Compile MPLDDepthwiseConv2dRelu");
		std::array<D3D12_ROOT_PARAMETER, 4> parameters{};
		for (UINT i = 0; i < parameters.size(); ++i) {
			parameters[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
			parameters[i].Descriptor.ShaderRegister = i;
		}
		D3D12_ROOT_SIGNATURE_DESC root{};
		root.NumParameters = static_cast<UINT>(parameters.size());
		root.pParameters = parameters.data();
		ComPtr<ID3DBlob> serialized;
		check(D3D12SerializeRootSignature(&root, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors), "Serialize MPLDDepthwiseConv2dRelu root signature");
		check(d3dDevice_->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&step.rootSignature)), "Create MPLDDepthwiseConv2dRelu root signature");
		D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
		desc.pRootSignature = step.rootSignature.Get();
		desc.CS = {bytecode->GetBufferPointer(), bytecode->GetBufferSize()};
		check(d3dDevice_->CreateComputePipelineState(&desc, IID_PPV_ARGS(&step.pipeline)), "Create MPLDDepthwiseConv2dRelu pipeline");
	}

	void recordStep(ID3D12GraphicsCommandList *commands, const Step &step)
	{
		if (step.output == noTensor)
			return;
		if (step.shader == CompiledShader::None) {
			recorder_->RecordDispatch(commands, step.operation.Get(), step.executionTable.Get());
			return;
		}
		commands->SetPipelineState(step.pipeline.Get());
		commands->SetComputeRootSignature(step.rootSignature.Get());
		for (UINT index = 0; index < step.inputs.size(); ++index) {
			const auto id = step.inputs[index];
			const auto &tensor = tensors_.at(id);
			commands->SetComputeRootUnorderedAccessView(index, resource(id)->GetGPUVirtualAddress() + (tensor.weight ? tensor.weightOffset : 0));
		}
		commands->SetComputeRootUnorderedAccessView(3, resource(step.output)->GetGPUVirtualAddress());
		commands->Dispatch(16 * 9 * 128 / 64, 1, 1);
	}

	void createExecutionTables()
	{
		for (std::size_t stepIndex = 0; stepIndex < steps_.size(); ++stepIndex) {
			auto &step = steps_[stepIndex];
			// Tensor IDs include explicit Node input barriers and node-internal barriers.
			for (const auto id : step.barriersBefore) {
				D3D12_RESOURCE_BARRIER barrier{};
				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
				barrier.UAV.pResource = resource(id);
				if (!barrier.UAV.pResource)
					throw std::runtime_error("UAV barrier target resource is unavailable");
				step.barriers.push_back(barrier);
			}
			if (step.output == noTensor || step.shader != CompiledShader::None)
				continue;
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
			if (step.operationProperties.TemporaryResourceSize) {
				const auto temporaryBuffer = bufferBinding(
					step.temporary.Get(), 0, step.operationProperties.TemporaryResourceSize);
				const auto temporaryBinding = bindingDesc(&temporaryBuffer);
				step.executionTable->BindTemporaryResource(&temporaryBinding);
			}
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
		TraceLoggingThreadActivity<g_hProvider, 0x1> submit;
		TraceLoggingWriteStart(submit, "Runtime.SubmitAndWait");
		check(dmlDevice_->GetDeviceRemovedReason(), "DirectML device before command submission");
		check(commandList_->Close(), "CommandList::Close");
		ID3D12CommandList *lists[] = {commandList_.Get()};
		queue_->ExecuteCommandLists(1, lists);
		const std::uint64_t value = ++fenceValue_;
		check(queue_->Signal(fence_.Get(), value), "CommandQueue::Signal");
		if (fence_->GetCompletedValue() < value) {
			TraceLoggingThreadActivity<g_hProvider, 0x1> wait;
			TraceLoggingWriteStart(wait, "Runtime.FenceWait");
			check(fence_->SetEventOnCompletion(value, fenceEvent_.get()), "Fence::SetEventOnCompletion");
			for (;;) {
				const auto result = WaitForSingleObject(fenceEvent_.get(), 1000);
				check(dmlDevice_->GetDeviceRemovedReason(),
				      "DirectML device removed while waiting for GPU");
				check(d3dDevice_->GetDeviceRemovedReason(),
				      "D3D12 device removed while waiting for GPU");
				if (result == WAIT_OBJECT_0)
					break;
				if (result != WAIT_TIMEOUT)
					throw std::runtime_error("DirectML GPU fence wait failed (result " +
								 std::to_string(result) + ")");
			}
		}
	}

	std::vector<Tensor> tensors_;
	std::vector<Storage> storages_;
	std::vector<Step> steps_;
	std::vector<TensorId> inputTensors_;
	TensorId outputTensor_ = noTensor;
	std::uint64_t weightBytes_ = 0;
	std::uint64_t outputSize_ = 0;
	ComPtr<ID3D12Device> d3dDevice_;
	ComPtr<IDMLDevice> dmlDevice_;
	ComPtr<ID3D12CommandQueue> queue_;
	ComPtr<ID3D12CommandAllocator> allocator_;
	ComPtr<ID3D12GraphicsCommandList> commandList_;
	ComPtr<IDMLCommandRecorder> recorder_;
	ComPtr<ID3D12DescriptorHeap> descriptorHeap_;
	std::uint32_t descriptorIncrement_ = 0;
	std::vector<ComPtr<ID3D12Resource>> inputUploads_;
	ComPtr<ID3D12Resource> readback_, weightUpload_, weight_;
	ComPtr<ID3D12Fence> fence_;
	UniqueHandle fenceEvent_;
	std::uint64_t fenceValue_ = 0;
};

BaselineDmlProgramRuntime::BaselineDmlProgramRuntime(const Graph &graph, std::span<const std::byte> weights)
	: impl_(std::make_unique<Impl>(graph, weights))
{
}
BaselineDmlProgramRuntime::~BaselineDmlProgramRuntime() noexcept = default;
void BaselineDmlProgramRuntime::run(std::span<const std::span<const std::byte>> inputs, std::span<std::byte> output)
{
	impl_->run(inputs, output);
}

} // namespace BaselineDml

namespace BaselineDml {
auto BaselineDmlProgramRuntime::device() const noexcept -> ID3D12Device *
{
	return impl_->device();
}
auto BaselineDmlProgramRuntime::queue() const noexcept -> ID3D12CommandQueue *
{
	return impl_->queue();
}
auto BaselineDmlProgramRuntime::inputCount() const noexcept -> std::size_t
{
	return impl_->inputCount();
}
auto BaselineDmlProgramRuntime::inputInfo(std::size_t index) const -> RuntimeTensorInfo
{
	return impl_->inputInfo(index);
}
auto BaselineDmlProgramRuntime::outputInfo() const noexcept -> RuntimeTensorInfo
{
	return impl_->outputInfo();
}
void BaselineDmlProgramRuntime::recordGpuRun(ID3D12GraphicsCommandList *commands, std::span<const GpuInput> inputs,
					     ID3D12Resource *predicate, ID3D12Resource *output, std::uint64_t offset)
{
	impl_->recordGpuRun(commands, inputs, predicate, output, offset);
}
} // namespace BaselineDml

#ifdef DML_MODEL_DIAGNOSTICS
namespace BaselineDml {
void BaselineDmlProgramRuntime::configureStorageDiagnostics(std::span<const std::uint8_t> roundedSteps)
{ impl_->configureStorageDiagnostics(roundedSteps); }
auto BaselineDmlProgramRuntime::diagnosticIntermediateValues() const -> const std::vector<std::vector<float>> &
{ return impl_->diagnosticIntermediateValues(); }
}
#endif
