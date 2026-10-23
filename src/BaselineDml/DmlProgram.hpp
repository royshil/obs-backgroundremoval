// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

struct IUnknown;
struct ID3D12Resource;
struct ID3D12DescriptorHeap;
struct ID3D12RootSignature;
struct ID3D12PipelineState;
struct IDMLCompiledOperator;
struct IDMLOperatorInitializer;
struct IDMLBindingTable;
struct IDMLCommandRecorder;
struct D3D12_RESOURCE_BARRIER;

namespace BaselineDml {
namespace Detail {
struct ComRelease final {
	void operator()(IUnknown *object) const noexcept;
};
template<typename T> using ComOwner = std::unique_ptr<T, ComRelease>;
using InternalTensors = std::unordered_map<std::uint64_t, ComOwner<ID3D12Resource>>;
struct Step;
} // namespace Detail

/**
 * Owns runtime resources directly. Destruction releases them; it neither submits work nor waits.
 * The caller must retain this owner until all recorded GPU work using its resources completes.
 * Definition metadata, devices, queues and synchronization are not stored here.
 * Accessors borrow resources without transferring ownership or extending their lifetime.
 */
class DmlProgram final {
public:
	DmlProgram() noexcept;
	DmlProgram(std::vector<Detail::ComOwner<ID3D12Resource>> storages, Detail::InternalTensors internalTensors,
		   std::vector<Detail::Step> steps, Detail::ComOwner<IDMLCommandRecorder> recorder,
		   Detail::ComOwner<ID3D12DescriptorHeap> initializationHeap,
		   Detail::ComOwner<ID3D12DescriptorHeap> descriptorHeap, Detail::ComOwner<ID3D12Resource> weightUpload,
		   Detail::ComOwner<ID3D12Resource> weight, std::vector<D3D12_RESOURCE_BARRIER> inputReadyTransitions,
		   std::vector<D3D12_RESOURCE_BARRIER> inputRestoreTransitions,
		   std::vector<D3D12_RESOURCE_BARRIER> outputTransitions) noexcept;
	~DmlProgram() noexcept;
	DmlProgram(const DmlProgram &) = delete;
	auto operator=(const DmlProgram &) -> DmlProgram & = delete;
	DmlProgram(DmlProgram &&) noexcept;
	auto operator=(DmlProgram &&) noexcept -> DmlProgram &;

	// Indexed access requires an index from the definition used to create this program.
	auto storage(std::size_t index) const noexcept -> ID3D12Resource *;
	auto commandRecorder() const noexcept -> IDMLCommandRecorder *;
	auto initializationHeap() const noexcept -> ID3D12DescriptorHeap *;
	auto descriptorHeap() const noexcept -> ID3D12DescriptorHeap *;
	auto weightUpload() const noexcept -> ID3D12Resource *;
	auto weight() const noexcept -> ID3D12Resource *;
	auto compiledOperator(std::size_t index) const noexcept -> IDMLCompiledOperator *;
	auto rootSignature(std::size_t index) const noexcept -> ID3D12RootSignature *;
	auto pipelineState(std::size_t index) const noexcept -> ID3D12PipelineState *;
	auto initializer(std::size_t index) const noexcept -> IDMLOperatorInitializer *;
	auto initializationTable(std::size_t index) const noexcept -> IDMLBindingTable *;
	auto executionTable(std::size_t index) const noexcept -> IDMLBindingTable *;
	auto stepBarriers(std::size_t index) const noexcept -> std::span<const D3D12_RESOURCE_BARRIER>;
	auto inputReadyTransitions() const noexcept -> std::span<const D3D12_RESOURCE_BARRIER>;
	auto inputRestoreTransitions() const noexcept -> std::span<const D3D12_RESOURCE_BARRIER>;
	auto outputTransitions() const noexcept -> std::span<const D3D12_RESOURCE_BARRIER>;
	// Returns null if the operation-local key has no allocated internal tensor.
	auto internalTensor(std::uint64_t key) const noexcept -> ID3D12Resource *;

#ifndef BASELINE_DML_FORCE_PUBLIC
private:
#endif
	std::vector<Detail::ComOwner<ID3D12Resource>> storages_;
	Detail::InternalTensors internalTensors_;
	std::vector<Detail::Step> steps_;
	Detail::ComOwner<IDMLCommandRecorder> recorder_;
	Detail::ComOwner<ID3D12DescriptorHeap> initializationHeap_;
	Detail::ComOwner<ID3D12DescriptorHeap> descriptorHeap_;
	Detail::ComOwner<ID3D12Resource> weightUpload_;
	Detail::ComOwner<ID3D12Resource> weight_;
	std::vector<D3D12_RESOURCE_BARRIER> inputReadyTransitions_;
	std::vector<D3D12_RESOURCE_BARRIER> inputRestoreTransitions_;
	std::vector<D3D12_RESOURCE_BARRIER> outputTransitions_;
};
} // namespace BaselineDml
