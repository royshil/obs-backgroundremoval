// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "DmlProgram.hpp"
#include "DmlProgramDefinition.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>

struct ID3D12Device;
struct IDMLDevice;
struct ID3D12GraphicsCommandList;
struct ID3D12Resource;

namespace BaselineDml {

struct TensorInfo final {
	std::uint32_t type;
	std::uint64_t dataBytes;
};

struct GpuInput final {
	ID3D12Resource *resource;
	std::uint64_t offset;
	std::uint64_t dataBytes;
};

constexpr auto inputCount(const DmlProgramDefinition &definition) noexcept -> std::size_t
{
	return definition.params.inputTensors.size();
}

constexpr auto inputInfo(const DmlProgramDefinition &definition, std::size_t index) noexcept -> TensorInfo
{
	assert(index < inputCount(definition));
	const auto &spec = definition.params.tensors[definition.params.inputTensors[index]];
	return {Definition::tensorDataType(spec), Definition::tensorDataBytes(spec)};
}

constexpr auto outputInfo(const DmlProgramDefinition &definition) noexcept -> TensorInfo
{
	const auto &spec = definition.params.tensors[definition.params.outputTensor];
	return {Definition::tensorDataType(spec), Definition::tensorDataBytes(spec)};
}

/**
 * Creates runtime resources from immutable nodes, constexpr-derived tables and prepacked weights.
 * No GPU commands are recorded or submitted. The returned DmlProgram owns every created resource.
 * Devices and the definition backing arrays remain caller-owned. Use the same definition and devices
 * when recording initialization and execution. The backing arrays must remain alive during recording.
 * SDK-dependent binding requirements are used during creation and are not retained as model metadata.
 * Throws DmlCompilationError for compilation failures, invalid_argument for an invalid definition,
 * runtime_error for device/resource failures, or bad_alloc for allocation failures.
 */
auto createDmlProgram(ID3D12Device *device, IDMLDevice *dmlDevice, const DmlProgramDefinition &definition)
	-> DmlProgram;

/**
 * Records one-time weight upload and operator initialization; does not submit or wait.
 * commands must be an open DIRECT list on the creation device with no active predication.
 * Execute initialization exactly once before inference, either in the same list or an ordered submission.
 * Never replay a list containing initialization. Keep the program, allocator and referenced resources alive
 * through GPU completion. Devices, queues, command lists, allocators and fences belong to the caller.
 */
void recordInitialization(const DmlProgramDefinition &definition, const DmlProgram &program,
			  ID3D12GraphicsCommandList *commands) noexcept;

/**
 * Records complete inference and an output copy; performs no allocation, submission or wait.
 * Inputs follow runtime InputItem node order, excluding weights. Each input is COPY_SOURCE and includes
 * its four-byte-rounded TensorSpec size; dataBytes is the unpadded byte count.
 * Output is a DEFAULT or READBACK buffer in COPY_DEST with room at outputOffset.
 * A non-null predicate provides eight bytes at offset zero in PREDICATION; zero preserves output.
 *
 * Initialization must execute first. A closed execution-only list can be replayed without recording again.
 * Wait for previous GPU execution before changing CPU-written input/predicate contents or resubmitting.
 * Keep program resources and the allocator's recorded contents alive for every execution; resetting the
 * allocator invalidates its recordings. Re-record when resource identities or offsets change.
 * Calls and executions using the same program must not overlap; cross-queue ordering belongs to the caller.
 * Retain all referenced GPU resources through completion, including submission error paths.
 *
 * External resource states are unchanged and internal states are restored for the next execution.
 * Recording changes descriptor heaps and compute state and clears predication; rebind subsequent work.
 */
void recordGpuRun(const DmlProgramDefinition &definition, const DmlProgram &program,
		  ID3D12GraphicsCommandList *commands, std::span<const GpuInput> inputs, ID3D12Resource *predicate,
		  ID3D12Resource *output, std::uint64_t outputOffset) noexcept;

} // namespace BaselineDml
