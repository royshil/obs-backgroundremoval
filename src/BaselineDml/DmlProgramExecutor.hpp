// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "DmlProgram.hpp"
#include "DmlProgramDefinition.hpp"
#include <memory>

struct ID3D12Device;
struct IDMLDevice;
struct ID3D12GraphicsCommandList;

namespace BaselineDml {

struct GpuInput final {
	ID3D12Resource *resource;
	std::uint64_t offset;
	std::uint64_t dataBytes;
};

/**
 * Rebuildable command-recording session borrowing a stable DmlProgram and model backing arrays.
 * Construction creates its own descriptor heaps, binding tables, recorder, DML temporary buffers and resolved barrier cache;
 * it never recompiles operators, reallocates program buffers, submits work or initializes GPU state.
 * DmlProgram and the definition backing arrays must outlive this session.
 * Retain this session through completion of every GPU execution using its descriptor heaps,
 * including replays of recorded command lists. Drain old work before destroying an old session.
 * Compiler returns an initialized program; recreating a session never repeats initialization.
 * Devices, command lists, queues, allocators, submission and synchronization belong to the caller.
 */
class DmlProgramExecutor final {
public:
	DmlProgramExecutor(ID3D12Device *device, IDMLDevice *dmlDevice, const DmlProgramDefinition &definition,
			   const DmlProgram &program);
	~DmlProgramExecutor() noexcept;
	DmlProgramExecutor(const DmlProgramExecutor &) = delete;
	auto operator=(const DmlProgramExecutor &) -> DmlProgramExecutor & = delete;

	/**
 * Records complete inference and an output copy; performs no allocation, submission or wait.
 * Inputs follow runtime InputItem node order, excluding weights. Each input is COPY_SOURCE and includes
 * its four-byte-rounded TensorSpec size; dataBytes is the unpadded byte count.
 * Output is a DEFAULT or READBACK buffer in COPY_DEST with room at outputOffset.
 * A non-null predicate provides eight bytes at offset zero in PREDICATION; zero preserves output.
 *
 * Compiler initialization must have completed first. A closed execution-only list can be replayed without recording again.
 * Wait for previous GPU execution before changing CPU-written input/predicate contents or resubmitting.
 * Keep this session, program resources and the allocator's recorded contents alive for every execution; resetting the
 * allocator invalidates its recordings. Re-record when resource identities or offsets change.
 * Calls and executions using the same program must not overlap; cross-queue ordering belongs to the caller.
 * Retain all referenced GPU resources through completion, including submission error paths.
 *
 * External resource states are unchanged and internal states are restored for the next execution.
 * Recording changes descriptor heaps and compute state and clears predication; rebind subsequent work.
 */
	void recordGpuRun(ID3D12GraphicsCommandList *commands, std::span<const GpuInput> inputs,
			  ID3D12Resource *predicate, ID3D12Resource *output, std::uint64_t outputOffset) const noexcept;

#ifndef BASELINE_DML_FORCE_PUBLIC
private:
#endif
	struct Impl;
	std::unique_ptr<Impl> impl_;
};
} // namespace BaselineDml
