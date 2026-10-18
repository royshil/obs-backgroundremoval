// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include <vector>

struct IUnknown;
struct ID3D12Resource;
struct IDMLCompiledOperator;
struct ID3D12RootSignature;
struct ID3D12PipelineState;

namespace BaselineDml {

/**
 * Transfer ownership with std::unique_ptr<DmlProgram> to preserve the address borrowed by executors.
 * Owns each node's output tensor reference and per-node DML persistent resources returned by DmlProgramCompiler.
 * Destruction releases them; it neither submits work nor waits.
 * The caller must retain this owner until all recorded GPU work using its resources completes.
 * Definition metadata, devices, queues and synchronization are not stored here.
 * tensorResources and dmlPersistentResources are indexed by NodeIndex. Reshape, Expand and weight nodes may share
 * an output resource through shared ownership. Executors borrow resources from tensorResources.
 * Callers preserve resource ownership and bindings until all recorded GPU work completes.
 */
struct DmlProgram final {
	template<typename T> using NodeVector = std::vector<T>;

	NodeVector<std::shared_ptr<ID3D12Resource>> tensorResources;
	NodeVector<std::unique_ptr<ID3D12Resource, void (*)(IUnknown *)>> dmlPersistentResources;
	NodeVector<std::unique_ptr<IDMLCompiledOperator, void (*)(IUnknown *)>> operations;
	NodeVector<std::unique_ptr<ID3D12RootSignature, void (*)(IUnknown *)>> rootSignatures;
	NodeVector<std::unique_ptr<ID3D12PipelineState, void (*)(IUnknown *)>> pipelines;

	// Client-owned upload staging. Compiler::endProgram waits for upload completion; reset when convenient.
	std::unique_ptr<ID3D12Resource, void (*)(IUnknown *)> weightUpload{nullptr, nullptr};
};

} // namespace BaselineDml
