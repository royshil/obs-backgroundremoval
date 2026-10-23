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
#include "Detail.hpp"
#include "DmlProgram.hpp"
#include <utility>

namespace BaselineDml {
void Detail::ComRelease::operator()(IUnknown *object) const noexcept
{
	object->Release();
}

DmlProgram::DmlProgram() noexcept = default;
DmlProgram::~DmlProgram() noexcept = default;
DmlProgram::DmlProgram(DmlProgram &&) noexcept = default;
auto DmlProgram::operator=(DmlProgram &&) noexcept -> DmlProgram & = default;
DmlProgram::DmlProgram(std::vector<Detail::ComOwner<ID3D12Resource>> storages, Detail::InternalTensors internalTensors,
		       std::vector<Detail::Step> steps, Detail::ComOwner<IDMLCommandRecorder> recorder,
		       Detail::ComOwner<ID3D12DescriptorHeap> initializationHeap,
		       Detail::ComOwner<ID3D12DescriptorHeap> descriptorHeap,
		       Detail::ComOwner<ID3D12Resource> weightUpload, Detail::ComOwner<ID3D12Resource> weight,
		       std::vector<D3D12_RESOURCE_BARRIER> inputReadyTransitions,
		       std::vector<D3D12_RESOURCE_BARRIER> inputRestoreTransitions,
		       std::vector<D3D12_RESOURCE_BARRIER> outputTransitions) noexcept
	: storages_(std::move(storages)),
	  internalTensors_(std::move(internalTensors)),
	  steps_(std::move(steps)),
	  recorder_(std::move(recorder)),
	  initializationHeap_(std::move(initializationHeap)),
	  descriptorHeap_(std::move(descriptorHeap)),
	  weightUpload_(std::move(weightUpload)),
	  weight_(std::move(weight)),
	  inputReadyTransitions_(std::move(inputReadyTransitions)),
	  inputRestoreTransitions_(std::move(inputRestoreTransitions)),
	  outputTransitions_(std::move(outputTransitions))
{
}
auto DmlProgram::storage(std::size_t index) const noexcept -> ID3D12Resource *
{
	return storages_[index].get();
}
auto DmlProgram::commandRecorder() const noexcept -> IDMLCommandRecorder *
{
	return recorder_.get();
}
auto DmlProgram::initializationHeap() const noexcept -> ID3D12DescriptorHeap *
{
	return initializationHeap_.get();
}
auto DmlProgram::descriptorHeap() const noexcept -> ID3D12DescriptorHeap *
{
	return descriptorHeap_.get();
}
auto DmlProgram::weightUpload() const noexcept -> ID3D12Resource *
{
	return weightUpload_.get();
}
auto DmlProgram::weight() const noexcept -> ID3D12Resource *
{
	return weight_.get();
}
auto DmlProgram::compiledOperator(std::size_t index) const noexcept -> IDMLCompiledOperator *
{
	return steps_[index].operation.get();
}
auto DmlProgram::rootSignature(std::size_t index) const noexcept -> ID3D12RootSignature *
{
	return steps_[index].rootSignature.get();
}
auto DmlProgram::pipelineState(std::size_t index) const noexcept -> ID3D12PipelineState *
{
	return steps_[index].pipeline.get();
}
auto DmlProgram::initializer(std::size_t index) const noexcept -> IDMLOperatorInitializer *
{
	return steps_[index].initializer.get();
}
auto DmlProgram::initializationTable(std::size_t index) const noexcept -> IDMLBindingTable *
{
	return steps_[index].initializationTable.get();
}
auto DmlProgram::executionTable(std::size_t index) const noexcept -> IDMLBindingTable *
{
	return steps_[index].executionTable.get();
}
auto DmlProgram::stepBarriers(std::size_t index) const noexcept -> std::span<const D3D12_RESOURCE_BARRIER>
{
	return steps_[index].barriers;
}
auto DmlProgram::inputReadyTransitions() const noexcept -> std::span<const D3D12_RESOURCE_BARRIER>
{
	return inputReadyTransitions_;
}
auto DmlProgram::inputRestoreTransitions() const noexcept -> std::span<const D3D12_RESOURCE_BARRIER>
{
	return inputRestoreTransitions_;
}
auto DmlProgram::outputTransitions() const noexcept -> std::span<const D3D12_RESOURCE_BARRIER>
{
	return outputTransitions_;
}
auto DmlProgram::internalTensor(std::uint64_t key) const noexcept -> ID3D12Resource *
{
	const auto found = internalTensors_.find(key);
	return found == internalTensors_.end() ? nullptr : found->second.get();
}
} // namespace BaselineDml
