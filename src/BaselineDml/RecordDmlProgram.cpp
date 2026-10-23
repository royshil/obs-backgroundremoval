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

#include "DmlProgramOperations.hpp"
#include "Detail.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>

using namespace BaselineDml::Definition;

namespace BaselineDml::Detail {

// Call-scoped recorder; borrows the immutable description and the resource owner.
class ProgramRecorder final {
public:
	ProgramRecorder(const DmlProgramDefinition &definition, const DmlProgram &program)
		: definition_(definition),
		  program_(program)
	{
#ifndef NDEBUG
		auto *resource = program_.weight() ? program_.weight() : program_.storage(0);
		ID3D12Device *device = nullptr;
		const auto result = resource->GetDevice(IID_PPV_ARGS(&device));
		d3dDevice_.reset(device);
		assert(SUCCEEDED(result));
#endif
	}

	void recordInitialization(ID3D12GraphicsCommandList *commands) const noexcept
	{
#ifndef NDEBUG
		assertValidCommands(commands);
#endif
		if (definition_.params.weightBytes) {
			commands->CopyBufferRegion(program_.weight(), 0, program_.weightUpload(), 0,
						   definition_.params.weightBytes);
			const auto barrier = transition(program_.weight(), D3D12_RESOURCE_STATE_COPY_DEST,
							D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			commands->ResourceBarrier(1, &barrier);
		}
		ID3D12DescriptorHeap *heaps[]{program_.initializationHeap()};
		commands->SetDescriptorHeaps(1, heaps);
		for (std::size_t index = 0; index < definition_.params.steps.size(); ++index) {
			if (!program_.initializer(index))
				continue;
			program_.commandRecorder()->RecordDispatch(commands, program_.initializer(index),
								   program_.initializationTable(index));
			D3D12_RESOURCE_BARRIER barrier{};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			commands->ResourceBarrier(1, &barrier);
		}
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
		if (!program_.inputRestoreTransitions().empty())
			commands->ResourceBarrier(static_cast<UINT>(program_.inputRestoreTransitions().size()),
						  program_.inputRestoreTransitions().data());
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
		ComOwner<ID3D12Device> owner;
		ID3D12Device *ownerPointer = nullptr;
		const HRESULT result = buffer->GetDevice(IID_PPV_ARGS(&ownerPointer));
		owner.reset(ownerPointer);
		assert(SUCCEEDED(result));
		assert(owner.get() == d3dDevice_.get());
		assert(desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER);
		assert(offset <= desc.Width && bytes <= desc.Width - offset);
	}

	void assertValidCommands(ID3D12GraphicsCommandList *commands) const noexcept
	{
		assert(commands != nullptr);
		assert(commands->GetType() == D3D12_COMMAND_LIST_TYPE_DIRECT);
		ComOwner<ID3D12Device> owner;
		ID3D12Device *ownerPointer = nullptr;
		const HRESULT result = commands->GetDevice(IID_PPV_ARGS(&ownerPointer));
		owner.reset(ownerPointer);
		assert(SUCCEEDED(result));
		assert(owner.get() == d3dDevice_.get());
	}
#endif

	void recordInputCopies(ID3D12GraphicsCommandList *commands, std::span<const GpuInput> inputs) const noexcept
	{
		for (std::size_t index = 0; index < inputs.size(); ++index) {
			const auto id = definition_.params.inputTensors[index];
			commands->CopyBufferRegion(resource(id), 0, inputs[index].resource, inputs[index].offset,
						   tensorSize(definition_.params.tensors[id]));
		}
		if (!program_.inputReadyTransitions().empty())
			commands->ResourceBarrier(static_cast<UINT>(program_.inputReadyTransitions().size()),
						  program_.inputReadyTransitions().data());
	}

	void recordExecution(ID3D12GraphicsCommandList *commands) const noexcept
	{
		ID3D12DescriptorHeap *heaps[]{program_.descriptorHeap()};
		commands->SetDescriptorHeaps(1, heaps);
		for (std::size_t index = 0; index < definition_.params.steps.size(); ++index) {
			const auto barriers = program_.stepBarriers(index);
			if (!barriers.empty())
				commands->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
			recordStep(commands, definition_.params.steps[index], index);
		}
	}

	void recordOutputCopy(ID3D12GraphicsCommandList *commands, ID3D12Resource *output,
			      std::uint64_t outputOffset) const noexcept
	{
		const auto &tensor = definition_.params.tensors[definition_.params.outputTensor];
		commands->ResourceBarrier(1, &program_.outputTransitions()[0]);
		commands->CopyBufferRegion(output, outputOffset, resource(definition_.params.outputTensor),
					   static_cast<std::uint64_t>(tensorByteOffset(tensor)),
					   BaselineDml::outputInfo(definition_).dataBytes);
		commands->ResourceBarrier(1, &program_.outputTransitions()[1]);
	}

	auto resource(std::int64_t id) const noexcept -> ID3D12Resource *
	{
		const auto &tensor = definition_.params.tensors[id];
		return isWeightTensor(tensor) ? program_.weight() : program_.storage(definition_.params.placements[id]);
	}

	void recordStep(ID3D12GraphicsCommandList *commands, const StepDefinition &description,
			std::size_t stepIndex) const noexcept
	{
		if (description.output == noTensor)
			return;
		if (description.shader == CompiledShader::None) {
			program_.commandRecorder()->RecordDispatch(commands, program_.compiledOperator(stepIndex),
								   program_.executionTable(stepIndex));
			return;
		}
		const auto inputs = std::span(description.inputs).first(description.inputCount);
		commands->SetPipelineState(program_.pipelineState(stepIndex));
		commands->SetComputeRootSignature(program_.rootSignature(stepIndex));
		for (UINT index = 0; index < inputs.size(); ++index) {
			const auto id = inputs[index];
			const auto &tensor = definition_.params.tensors[id];
			commands->SetComputeRootUnorderedAccessView(
				index, resource(id)->GetGPUVirtualAddress() +
					       (static_cast<std::uint64_t>(tensorByteOffset(tensor))));
		}
		commands->SetComputeRootUnorderedAccessView(3, resource(description.output)->GetGPUVirtualAddress());
		if (description.shader == CompiledShader::MPLD10DepthwiseConv2dRelu)
			commands->Dispatch(16 * 9 * 128 / 64, 1, 1);
		else if (description.shader == CompiledShader::MPLD10BinarizeFloat32)
			commands->Dispatch((256 * 144 + 63) / 64, 1, 1);
	}

	const DmlProgramDefinition &definition_;
	const DmlProgram &program_;
#ifndef NDEBUG
	ComOwner<ID3D12Device> d3dDevice_;
#endif
};

} // namespace BaselineDml::Detail

namespace BaselineDml {

void recordInitialization(const DmlProgramDefinition &definition, const DmlProgram &program,
			  ID3D12GraphicsCommandList *commands) noexcept
{
	assert(program.commandRecorder() != nullptr);
	Detail::ProgramRecorder(definition, program).recordInitialization(commands);
}

void recordGpuRun(const DmlProgramDefinition &definition, const DmlProgram &program,
		  ID3D12GraphicsCommandList *commands, std::span<const GpuInput> inputs, ID3D12Resource *predicate,
		  ID3D12Resource *output, std::uint64_t outputOffset) noexcept
{
	assert(program.commandRecorder() != nullptr);
	Detail::ProgramRecorder(definition, program).recordGpuRun(commands, inputs, predicate, output, outputOffset);
}

} // namespace BaselineDml
