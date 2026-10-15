// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "BaselineDmlProgramDefinition.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#ifdef DML_MODEL_DIAGNOSTICS
#include <vector>
#endif

struct ID3D12Device;
struct ID3D12CommandQueue;
struct ID3D12GraphicsCommandList;
struct ID3D12Resource;

namespace BaselineDml {

// Execution selection is separate from the static operator/node definitions.
struct Graph final {
	std::size_t inputSpecCount;
	const ProgramDefinition::InputSpecItem *inputSpecs;
	std::int64_t nodeCount;
	const ProgramDefinition::Node *nodes;
	std::size_t outputIndex;
	// Row i: producer Node outputs to synchronize before executing Node i.
	// -1 marks an unused slot. Null means no Node-boundary UAV barriers.
	const std::array<std::int32_t, 8> *uavBarriers = nullptr;
};

struct RuntimeTensorInfo final {
	std::uint32_t type;
	std::uint64_t dataBytes;
};

struct GpuInput final {
	ID3D12Resource *resource;
	std::uint64_t offset;
	std::uint64_t dataBytes;
};

class BaselineDmlProgramRuntime final {
public:
	explicit BaselineDmlProgramRuntime(const Graph &graph, std::span<const std::byte> weights);
	~BaselineDmlProgramRuntime() noexcept;

	BaselineDmlProgramRuntime(const BaselineDmlProgramRuntime &) = delete;
	auto operator=(const BaselineDmlProgramRuntime &) -> BaselineDmlProgramRuntime & = delete;
	BaselineDmlProgramRuntime(BaselineDmlProgramRuntime &&) = delete;
	auto operator=(BaselineDmlProgramRuntime &&) -> BaselineDmlProgramRuntime & = delete;

	// Inputs follow RuntimeInput entries in the specification table, excluding weights.
	// Spans contain packed elements, without DirectML's buffer padding.
	void run(std::span<const std::span<const std::byte>> inputs, std::span<std::byte> output);

	// Borrowed interfaces. GPU submissions must use this device and queue, and
	// finish before the next run/recordGpuRun or destruction of the runtime.
	auto device() const noexcept -> ID3D12Device *;
	auto queue() const noexcept -> ID3D12CommandQueue *;
	auto inputCount() const noexcept -> std::size_t;
	auto inputInfo(std::size_t index) const -> RuntimeTensorInfo;
	auto outputInfo() const noexcept -> RuntimeTensorInfo;
#ifdef DML_MODEL_DIAGNOSTICS
 // Test-only FLOAT32 storage-rounding experiment. Absent from production builds.
 void configureStorageDiagnostics(std::span<const std::uint8_t> roundedSteps);
 auto diagnosticIntermediateValues() const -> const std::vector<std::vector<float>> &;
#endif


	// Inputs are padded GPU buffers in COPY_SOURCE; predicate, when non-null, is an eight-byte
	// value in PREDICATION. Output is a READBACK buffer in COPY_DEST. This only
	// records work: the caller submits and waits. A zero predicate leaves the
	// output readback unchanged. Input resources remain in COPY_SOURCE.
	void recordGpuRun(ID3D12GraphicsCommandList *commands, std::span<const GpuInput> inputs,
			  ID3D12Resource *predicate, ID3D12Resource *output, std::uint64_t outputOffset);

private:
	class Impl;
	std::unique_ptr<Impl> impl_;
};

} // namespace BaselineDml
