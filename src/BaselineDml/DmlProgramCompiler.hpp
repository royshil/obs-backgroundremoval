// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <exception>
#include <memory>

#include "DmlProgramDefinition.hpp"
#include "DmlProgram.hpp"

struct IDMLDevice;

namespace BaselineDml {

/**
 * Statically builds one DmlProgram. Every phase is synchronous CPU-side Direct3D and DirectML object creation; the
 * compiler records no commands, owns no queue or fence, submits nothing and never waits for the GPU.
 *
 * @code
 * DmlProgramCompiler compiler(dmlDevice, definition);
 * for (std::size_t index = 0; index < definition.nodes.size(); ++index) {
 *     compiler.beginNode(index);
 *     std::visit(compiler, definition.nodes[index]);
 *     compiler.endNode(index);
 * }
 * auto program = compiler.build();
 *
 * DmlProgramInitializer initializer(device, dmlDevice, definition, *program);
 * initializer.recordInitialization(commands);
 * // The caller submits commands and waits for completion, then may release initializer and program->weightUpload.
 * @endcode
 *
 * Call sequence is a caller-enforced contract: for each node in definition order, call beginNode(index), visit that
 * node exactly once, then call endNode(index); call build exactly once after all nodes. Do not interleave or repeat
 * these phases. The compiler does not perform runtime phase tracking.
 *
 * Construction allocates the packed GPU weight buffer and an upload buffer already holding the weight bytes. Each leaf
 * operator() creates its own output resource from the current node's resolved TensorSpec; Reshape and Expand share
 * their input resource directly. Each custom-shader node visitor creates its shader resources. Each DirectML operator
 * visitor compiles its operator and allocates the persistent storage it requires.
 * build creates a fresh DmlProgram and transfers resource ownership; the compiler never holds a Program.
 * The returned program is not executable until DmlProgramInitializer's recorded initialization has completed.
 * No inference or warmup is performed. Discard the compiler on failure; phases are not retryable.
 * Model arrays/devices remain caller-owned and must outlive compilation; arrays must also outlive executors.
 */
class DmlProgramCompiler final {
public:
	using HRESULT = long;

	struct WinError : public std::runtime_error {
		WinError(const char *message_, HRESULT hr_) : std::runtime_error(message_), result(hr_) {}
		WinError(const std::string &message_, HRESULT hr_) : std::runtime_error(message_), result(hr_) {}
		HRESULT result;
	};

	/**
	 * @param device DirectML device used to create program resources.
	 * @param definition Non-empty model node sequence and non-empty source weight data with prepared tables matching
	 *                   the node sequence.
	 * @throws std::invalid_argument if device is null, nodes or weights are empty, weight sizes disagree, or a
	 *                               node-indexed parameter table has a different size from nodes.
	 * @throws DmlProgramCompilerError if DirectML or Direct3D resource preparation fails.
	 * The caller retains ownership of device and definition storage; they must outlive this compiler's build process.
	 */
	DmlProgramCompiler(IDMLDevice *device, const DmlProgramDefinition &definition);
	~DmlProgramCompiler() noexcept;

	DmlProgramCompiler(const DmlProgramCompiler &) = delete;
	auto operator=(const DmlProgramCompiler &) -> DmlProgramCompiler & = delete;
	DmlProgramCompiler(DmlProgramCompiler &&) = delete;
	auto operator=(DmlProgramCompiler &&) -> DmlProgramCompiler & = delete;

	/**
	 * @param nodeIndex Index of the next node in definition order.
	 * @pre This node has not been visited; nodeIndex is valid and is the next definition index.
	 * @post Selects nodeIndex as the destination for the following visitor call.
	 * @throws std::logic_error if nodeIndex is out of range.
	 * Call once immediately before visiting the indexed node. Do not nest or interleave node visits.
	 */
	void beginNode(std::int64_t nodeIndex);

	/**
	 * @param nodeIndex Index of the node whose visitor was just called.
	 * @pre beginNode(nodeIndex) selected this node and its visitor call completed.
	 * @post The matching node selection is cleared.
	 * @throws std::logic_error if nodeIndex does not match the active node.
	 * Call once immediately after the matching visitor call, before beginning the next node.
	 */
	void endNode(std::int64_t nodeIndex);

	/**
	 * @pre Every definition node was visited in order, each enclosed by matching beginNode(index) and endNode(index)
	 *      calls, and build has not previously been called.
	 * @return A new uninitialized DmlProgram that owns the resources needed for initialization and execution.
	 * @post Ownership of program resources is transferred to the returned object; the compiler does not retain a
	 *       DmlProgram. The returned program may be used after the compiler is destroyed.
	 * @throws std::bad_alloc if allocating the DmlProgram or its ownership vectors fails.
	 * Call exactly once after the final endNode. The caller owns the returned program and may release it when no
	 * longer needed.
	 */
	auto build() -> std::unique_ptr<DmlProgram>;

	// MARK: - Visitors to build BaselineDml program

	auto operator()(const InputItem &node) -> void;
	auto operator()(const BiasedConv2dRelu &node) -> void;
	auto operator()(const HardSwishGate &node) -> void;
	auto operator()(const MPLD10DepthwiseConv2dRelu &node) -> void;
	auto operator()(const MPLD10BinarizeFloat32 &node) -> void;
	auto operator()(const AveragePool2d &node) -> void;
	auto operator()(const BiasedConv2d &node) -> void;
	auto operator()(const BiasedConvTranspose2d &node) -> void;
	auto operator()(const DepthToSpace &node) -> void;
	auto operator()(const ClipFloat32 &node) -> void;
	auto operator()(const Resize &node) -> void;
	auto operator()(const Sigmoid &node) -> void;
	auto operator()(const Reshape &node) -> void;
	auto operator()(const Expand &node) -> void;
	auto operator()(const Add &node) -> void;
	auto operator()(const Mul &node) -> void;
	auto operator()(const Relu &node) -> void;
	auto operator()(const FusedNode &nodes) -> void;
	auto operator()(const Onnx11Node &nodes) -> void;
	auto operator()(const Onnx13Node &nodes) -> void;
	auto operator()(const Onnx14Node &nodes) -> void;

#ifndef BASELINE_DML_FORCE_PUBLIC
private:
#endif
	struct Impl;

	static auto makeImpl(IDMLDevice *device, const DmlProgramDefinition &definition) -> std::unique_ptr<Impl>;

	const std::unique_ptr<Impl> impl_;
};

struct DmlProgramCompilerError final : std::exception {
	using HRESULT = long;

	const char *msg;
	HRESULT hresult;

	DmlProgramCompilerError(const char *msg_, HRESULT hresult_) noexcept : msg(msg_), hresult(hresult_) {}

	auto what() const noexcept -> const char * override { return msg; }
};

} // namespace BaselineDml
