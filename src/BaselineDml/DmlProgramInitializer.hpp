// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>

#include "DmlProgram.hpp"
#include "DmlProgramDefinition.hpp"

struct ID3D12Device;
struct IDMLDevice;
struct ID3D12GraphicsCommandList;

namespace BaselineDml {

/**
 * One-shot command-recording session that initializes a DmlProgram built by DmlProgramCompiler.
 * Construction resets a DirectML operator initializer with the program's compiled operators and creates its own
 * descriptor heap, binding table and DML temporary buffer; it never submits work or waits.
 * The program, its weightUpload, and the definition backing arrays must outlive this session.
 * Devices, command lists, queues, allocators, submission and synchronization belong to the caller.
 * Retain this session and program->weightUpload until the recorded initialization has completed on the GPU.
 */
class DmlProgramInitializer final {
public:
	/**
	 * @throws std::logic_error if the program has no DirectML operators.
	 * @throws std::invalid_argument if a device is null or the operator count exceeds the DirectML limit.
	 * @throws DmlProgramInitializerError if DirectML or Direct3D initialization setup fails.
	 */
	DmlProgramInitializer(ID3D12Device *device, IDMLDevice *dmlDevice, const DmlProgramDefinition &definition,
			      const DmlProgram &program);
	~DmlProgramInitializer() noexcept;
	DmlProgramInitializer(const DmlProgramInitializer &) = delete;
	auto operator=(const DmlProgramInitializer &) -> DmlProgramInitializer & = delete;

	/**
	 * Records the weight upload, its transition to UNORDERED_ACCESS, the DirectML initializer dispatch and a UAV
	 * barrier; performs no allocation, submission or wait.
	 *
	 * Record and submit exactly once per program; the recording is not replayable. The caller must wait for its
	 * completion before submitting any DmlProgramExecutor run of the program.
	 * Recording changes descriptor heaps and compute state; rebind subsequent work.
	 */
	void recordInitialization(ID3D12GraphicsCommandList *commands) const noexcept;

#ifndef BASELINE_DML_FORCE_PUBLIC
private:
#endif
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

struct DmlProgramInitializerError final : std::exception {
	using HRESULT = long;

	const char *msg;
	HRESULT hresult;

	DmlProgramInitializerError(const char *msg_, HRESULT hresult_) noexcept : msg(msg_), hresult(hresult_) {}

	auto what() const noexcept -> const char * override { return msg; }
};

} // namespace BaselineDml
