// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "DmlProgramDefinition.hpp"
#include "DmlProgram.hpp"

#include <cstddef>
#include <memory>
#include <span>

struct IDMLDevice;

namespace BaselineDml {

/**
 * @brief Creates device-dependent operators from nodes and a pre-resolved ModelParams.
 *
 * Input is a borrowed DirectML device, nodes, generated ModelParams and source weight bytes.
 * Tensor layouts, storage assignments, weight packing and UAV barrier targets are already resolved.
 * Every node has exactly one output tensor at the same index. Conv bias reshaping is SDK-internal.
 * Model and weight spans are consumed during construction and need not outlive it.
 * The device must outlive this compiler and the executor using its result. Output is owned per-operation resources and internal tensors, transferred to DmlProgram.
 *
 * Construction checks device support and creates DirectML operators. It does not transform model metadata,
 * allocate D3D12 buffers, initialize operators, record GPU commands, submit work or wait.
 * Executor prepares those resources.
 */
class DmlProgramCompiler final {
public:
	/**
	 * @pre nodes and the table come from successful model preparation and have not been modified,
	 *      except that params.outputTensor may select another node index.
	 * @throws DmlCompilationError If device support checks or DirectML operator compilation fail.
	 * @throws std::invalid_argument If device is null or the weight byte count differs from the model.
	 * @throws std::bad_alloc If memory allocation fails.
	 */
	DmlProgramCompiler(IDMLDevice *device, std::span<const Definition::Node> nodes,
			   const Definition::ModelParams &params, std::span<const std::byte> weights);
	~DmlProgramCompiler() noexcept;

	DmlProgramCompiler(const DmlProgramCompiler &) = delete;
	auto operator=(const DmlProgramCompiler &) -> DmlProgramCompiler & = delete;
	DmlProgramCompiler(DmlProgramCompiler &&) = delete;
	auto operator=(DmlProgramCompiler &&) -> DmlProgramCompiler & = delete;

	// One-time ownership transfers; Detail.hpp is required to use the returned steps.
	auto takeSteps() noexcept -> std::vector<Detail::Step>;
	auto takeInternalTensors() noexcept -> Detail::InternalTensors;

#ifndef BASELINE_DML_FORCE_PUBLIC
private:
#endif
	class Builder;
	std::vector<Detail::Step> steps_;
	Detail::InternalTensors internalTensors_;
};

} // namespace BaselineDml
