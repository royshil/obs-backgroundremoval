#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <TraceLoggingProvider.h>

TRACELOGGING_DECLARE_PROVIDER(g_hProvider);
// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Inspect compilation details without exposing them through the public API.
#include <BaselineDml/DmlProgramCompiler.hpp>
#include <BaselineDml/DmlProgramOperations.hpp>
#include <BaselineDml/Detail.hpp>
#include "ProgramTestSession.hpp"
#include "TensorSpecFixture.hpp"

#include <DirectML.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

using namespace BaselineDml;
using namespace BaselineDml::Detail;
using DmlProgramTest::ProgramTestSession;
using DmlProgramTest::makeInputTensorSpec;
using DmlProgramTest::makeWeightTensorSpec;
using namespace BaselineDml::Definition;
using Microsoft::WRL::ComPtr;

void checkDmlProgramHeaderIsolation();

namespace {
static_assert(std::is_aggregate_v<InputTensorSpec>);
static_assert(std::is_aggregate_v<WeightTensorSpec>);
// Explicit views share storage and record no dispatch; barriers retain their GPU writer identity.
constexpr std::array sidecarSpecs{makeInputTensorSpec(dtFloat32, 1, {1}), makeWeightTensorSpec(0, dtFloat32, 1, {2})};
constexpr std::array<Node, 5> sidecarNodes{InputItem{dtFloat32, 0}, InputItem{dtFloat32, 1},
					   Onnx13Node{Expand{dtFloat32, 0, 1, {2}}},
					   Onnx14Node{Add{dtFloat32, 2, 1, 1, {2}}},
					   Onnx14Node{Relu{dtFloat32, 3, 1, {2}}}};
constexpr std::array sidecarBarriers{std::pair<std::int32_t, std::int32_t>{4, 3}};
static_assert(makeUavBarriers<1>(sidecarNodes) == sidecarBarriers);
constexpr auto sidecarTables = makeModelParamsStorage(sidecarSpecs, sidecarNodes, sidecarBarriers, 4, 8);
constexpr auto sidecarModel = sidecarTables.params();
static_assert(sidecarModel.tensors.size() == sidecarNodes.size());
static_assert(sidecarModel.steps.size() == 2 && sidecarModel.storages.size() == 3);
static_assert(sidecarModel.steps[0].output == 3 && sidecarModel.steps[1].output == 4);
static_assert(std::get<InputTensorSpec>(sidecarModel.tensors[sidecarModel.steps[0].inputs[0]]).strides[0] == 0);
static_assert(std::get<InputTensorSpec>(sidecarModel.tensors[sidecarModel.steps[0].output]).dataBytes == 8);
static_assert(sidecarModel.uavBarriers[sidecarModel.steps[1].barrierBegin] == 3);
static_assert(sidecarModel.placements[sidecarModel.uavBarriers[0]] ==
	      sidecarModel.placements[sidecarModel.steps[0].output]);

constexpr std::array<char, 8> sidecarWeights{};
constexpr DmlProgramDefinition sidecarDefinition{sidecarNodes, sidecarModel, sidecarWeights};
static_assert(sidecarDefinition.params.tensors.data() == sidecarTables.tensors.data());
static_assert(sidecarDefinition.nodes.data() == sidecarNodes.data());

ComPtr<IDMLDevice> device;

void expect(bool condition, const char *message)
{
	if (!condition)
		throw std::runtime_error(message);
}

void expectError(auto &&function, std::string_view expected)
{
	try {
		function();
	} catch (const std::exception &error) {
		expect(std::string_view(error.what()).find(expected) != std::string_view::npos, error.what());
		return;
	}
	throw std::runtime_error("Expected error containing " + std::string(expected));
}

auto makeCompiler(IDMLDevice *dmlDevice, std::span<const TensorSpec> specs, std::span<const Node> nodes,
		  std::span<const std::pair<std::int32_t, std::int32_t>> barriers, std::int32_t outputIndex,
		  std::span<const std::byte> weights) -> DmlProgramCompiler
{
	if (!dmlDevice)
		throw std::invalid_argument("device is null");
	const auto tables = makeModelParamsStorage(specs, nodes, barriers, outputIndex, weights.size());
	return {dmlDevice, nodes, tables.params(), weights};
}

auto compile(std::span<const TensorSpec> tensorSpecs, std::span<const Node> graphNodes,
	     std::span<const std::pair<std::int32_t, std::int32_t>> uavBarriers, std::int32_t outputIndex,
	     std::span<const std::byte> weights = {}) -> PreparedParams
{
	auto compiler = makeCompiler(device.Get(), tensorSpecs, graphNodes, uavBarriers, outputIndex, weights);
	return prepareModelParams(tensorSpecs, graphNodes, uavBarriers, outputIndex, weights.size());
}
} // namespace

int testExplicitShapeNodes()
{
	expect(encodeInternalTensorId(1, 2) == 0x0000000100000002ULL &&
		       encodeInternalTensorId(0xffffffffu, 0xffffffffu) == 0xffffffffffffffffULL &&
		       encodeInternalTensorId(1, 0) != encodeInternalTensorId(0, 1),
	       "internal tensor owner/local IDs must have disjoint 32-bit fields");
	InternalTensors internalTensors;
	constexpr auto internalSpec = std::get<InputTensorSpec>(makeInputTensorSpec(dtFloat32, 1, {2}));
	constexpr std::array internalDefinitions{InternalTensorDefinition{encodeInternalTensorId(1, 0), internalSpec},
						 InternalTensorDefinition{encodeInternalTensorId(2, 0), internalSpec},
						 InternalTensorDefinition{encodeInternalTensorId(1, 1), internalSpec}};
	ComPtr<ID3D12Device> internalTensorDevice;
	DmlProgramTest::check(device->GetParentDevice(IID_PPV_ARGS(&internalTensorDevice)), "GetParentDevice");
	allocateInternalTensors(internalTensorDevice.Get(), internalDefinitions, internalTensors);
	expect(internalTensors.size() == 3 && internalTensors.at(encodeInternalTensorId(1, 0)) &&
		       internalTensors.at(encodeInternalTensorId(1, 0)).get() !=
			       internalTensors.at(encodeInternalTensorId(2, 0)).get() &&
		       internalTensors.at(encodeInternalTensorId(1, 0)).get() !=
			       internalTensors.at(encodeInternalTensorId(1, 1)).get(),
	       "internal tensor owner/local keys must retain distinct GPU allocations");
	expect(internalTensors.at(encodeInternalTensorId(1, 0))->GetDesc().Width == internalSpec.size,
	       "internal tensor allocation must follow its TensorSpec");
	const std::array specs{makeInputTensorSpec(dtFloat32, 1, {2})};
	const std::array<Node, 9> nodes{InputItem{dtFloat32, 0},
					Onnx13Node{Reshape{dtFloat32, 0, 4, {1, 2, 1, 1}}},
					Onnx13Node{Expand{dtFloat32, 1, 4, {1, 2, 2, 3}}},
					Onnx14Node{Relu{dtFloat32, 2, 4, {1, 2, 2, 3}}},
					Onnx13Node{Reshape{dtFloat32, 3, 2, {2, 6}}},
					Onnx13Node{Reshape{dtFloat32, 4, 4, {1, 2, 2, 3}}},
					Onnx13Node{Expand{dtFloat32, 5, 4, {1, 2, 2, 3}}},
					Onnx14Node{Add{dtFloat32, 3, 6, 4, {1, 2, 2, 3}}},
					Onnx14Node{Mul{dtFloat32, 7, 6, 4, {1, 2, 2, 3}}}};
	const auto barriers = makeUavBarriers<2>(nodes);
	expect(barriers[0] == std::pair<std::int32_t, std::int32_t>{7, 3} &&
		       barriers[1] == std::pair<std::int32_t, std::int32_t>{8, 7},
	       "views changed UAV writer identity");
	const auto plan = compile(specs, nodes, barriers, 8);
	const std::array unusedSpecs{specs[0], TensorSpec{InputTensorSpec{}}};
	const auto unused = compile(unusedSpecs, nodes, barriers, 8);
	expect(unused.tensors.size() == nodes.size() && unused.inputTensors.size() == 1 &&
		       unused.storages.size() == plan.storages.size(),
	       "unreferenced source specifications must not allocate tensors or storage");
	expect(plan.steps.size() == 3, "metadata nodes added GPU dispatches");
	expect(plan.tensors.size() == nodes.size() && plan.outputTensor == 8 && plan.steps[0].output == 3 &&
		       plan.steps[1].output == 7 && plan.steps[2].output == 8,
	       "node and tensor indices differ");
	ProgramTestSession runtime(specs, nodes, barriers, 8, {});
	const std::array<float, 2> input{-2, 3};
	const std::array<std::span<const std::byte>, 1> inputs{std::as_bytes(std::span(input))};
	std::array<float, 12> output{};
	runtime.run(inputs, std::as_writable_bytes(std::span(output)));
	for (std::size_t index = 0; index < output.size(); ++index)
		expect(output[index] == (index < 6 ? 0.0F : 18.0F), "explicit shape graph output differs");
	const std::array<Node, 2> implicitUnary{nodes[0], Onnx14Node{Relu{dtFloat32, 0, 4, {1, 2, 1, 1}}}};
	expectError([&] { compile(specs, implicitUnary, {}, 1); }, "shapes must match");
	const std::array<Node, 3> implicitAdd{nodes[0], nodes[1], Onnx14Node{Add{dtFloat32, 0, 1, 4, {1, 2, 1, 1}}}};
	expectError([&] { compile(specs, implicitAdd, {}, 2); }, "shapes must match");
	const std::array<Node, 2> badReshape{nodes[0], Onnx13Node{Reshape{dtFloat32, 0, 1, {3}}}};
	expectError([&] { compile(specs, badReshape, {}, 1); }, "element count");
	const std::array<Node, 2> badExpand{nodes[0], Onnx13Node{Expand{dtFloat32, 0, 1, {3}}}};
	expectError([&] { compile(specs, badExpand, {}, 1); }, "broadcast");
	const std::array<Node, 2> badExpandRank{nodes[0], Onnx13Node{Expand{dtFloat32, 0, 4, {1, 2, 1, 1}}}};
	expectError([&] { compile(specs, badExpandRank, {}, 1); }, "matching ranks");
	const std::array<Node, 4> noncontiguous{nodes[0], nodes[1], nodes[2],
						Onnx13Node{Reshape{dtFloat32, 2, 1, {12}}}};
	expectError([&] { compile(specs, noncontiguous, {}, 3); }, "contiguous input");
	return 0;
}

int testMultipleInputsAndReuse()
{
	const std::array specs{
		makeInputTensorSpec(dtFloat32, 4, {1, 1, 1, 3}),
		makeWeightTensorSpec(0, dtFloat32, 4, {1, 1, 1, 3}),
		makeInputTensorSpec(dtFloat32, 4, {1, 1, 1, 3}),
	};
	const Node nodes[]{InputItem{dtFloat32, 2},
			   InputItem{dtFloat32, 1},
			   InputItem{dtFloat32, 0},
			   InputItem{dtFloat32, 0},
			   Onnx14Node{Mul{dtFloat32, 2, 1, 4, {1, 1, 1, 3}}},
			   Onnx14Node{Add{dtFloat32, 4, 0, 4, {1, 1, 1, 3}}},
			   Onnx14Node{Add{dtFloat32, 5, 3, 4, {1, 1, 1, 3}}}};
	std::pair<std::int32_t, std::int32_t> uavBarriers[]{{5, 4}, {6, 5}};
	std::span<const TensorSpec> graphTensorSpecs = specs;
	std::span<const Node> graphNodes = std::span(nodes).subspan(0, static_cast<std::int32_t>(std::size(nodes)));
	std::span<const std::pair<std::int32_t, std::int32_t>> graphBarriers = uavBarriers;
	std::int32_t graphOutputIndex = 6;
	const std::array<float, 3> weights{2, 2, 2};
	const auto weightBytes = std::as_bytes(std::span(weights));
	const auto program = compile(graphTensorSpecs, graphNodes, graphBarriers, graphOutputIndex, weightBytes);
	expect(program.inputTensors == std::vector<std::int64_t>{0, 2, 3}, "inputs do not follow InputItem order");
	expect(program.tensors.size() == std::size(nodes), "tensor count differs from node count");
	const auto &reused = program.placements.at(program.steps.back().inputs[1]);
	expect(reused == program.placements.at(3) && reused != program.placements.at(2),
	       "distinct InputItem nodes must own distinct input storage");
	ProgramTestSession runtime(graphTensorSpecs, graphNodes, graphBarriers, graphOutputIndex, weightBytes);
	for (int iteration = 1; iteration <= 2; ++iteration) {
		const std::array<float, 3> a{float(iteration), 2, 3}, b{10, 20, 30};
		std::array<float, 3> output{};
		const std::array<std::span<const std::byte>, 3> inputs{
			std::as_bytes(std::span(b)), std::as_bytes(std::span(a)), std::as_bytes(std::span(a))};
		runtime.run(inputs, std::as_writable_bytes(std::span(output)));
		for (std::size_t i = 0; i < output.size(); ++i)
			expect(std::abs(output[i] - (3 * a[i] + b[i])) < 1e-5F, "multiple input arithmetic differs");
		expectError([&] { runtime.run(std::span(inputs).first(1), std::as_writable_bytes(std::span(output))); },
			    "input count");
		const std::array wrongInputs{inputs[0], inputs[1].first(4), inputs[2]};
		expectError([&] { runtime.run(wrongInputs, std::as_writable_bytes(std::span(output))); },
			    "input 1 byte size");
		expectError([&] { runtime.run(inputs, std::as_writable_bytes(std::span(output)).first(4)); },
			    "output byte size");
	}
	return 0;
}

int testTypedPaddingAndPassthrough()
{
	const std::array types{dtFloat32, dtFloat16, dtUInt32,  dtUInt16, dtUInt8, dtInt32,
			       dtInt16,   dtInt8,    dtFloat64, dtUInt64, dtInt64};
	const std::array<std::size_t, 11> widths{4, 2, 4, 2, 1, 4, 2, 1, 8, 8, 8};
	for (std::size_t i = 0; i < types.size(); ++i) {
		const auto spec = makeInputTensorSpec(types[i], 1, {3});
		const Node nodes[]{InputItem{static_cast<std::int32_t>(types[i]), 0},
				   InputItem{static_cast<std::int32_t>(types[i]), 0}};
		std::span<const TensorSpec> graphTensorSpecs = std::span(&spec, 1);
		std::span<const Node> graphNodes = std::span(nodes).subspan(0, 2);
		std::span<const std::pair<std::int32_t, std::int32_t>> graphBarriers = {};
		std::int32_t graphOutputIndex = 1;
		DML_FEATURE_QUERY_TENSOR_DATA_TYPE_SUPPORT query{static_cast<DML_TENSOR_DATA_TYPE>(types[i])};
		DML_FEATURE_DATA_TENSOR_DATA_TYPE_SUPPORT support{};
		expect(SUCCEEDED(device->CheckFeatureSupport(DML_FEATURE_TENSOR_DATA_TYPE_SUPPORT, sizeof(query),
							     &query, sizeof(support), &support)),
		       "type support query failed");
		if (!support.IsSupported) {
			expectError([&] { compile(graphTensorSpecs, graphNodes, graphBarriers, graphOutputIndex); },
				    "unsupported by the DirectML device");
			std::cout << "Device rejects data type " << static_cast<unsigned>(types[i]) << '\n';
			continue;
		}
		const auto program = compile(graphTensorSpecs, graphNodes, graphBarriers, graphOutputIndex);
		expect(program.tensors.size() == std::size(nodes) && program.storages.size() == 2,
		       "input tensors must be placed by node, not by source specification");
		expect(tensorDataBytes(program.tensors[0]) == 3 * widths[i], "incorrect element width");
		expect(tensorSize(program.tensors[0]) == (3 * widths[i] + 3) / 4 * 4, "incorrect buffer padding");
		std::vector<std::byte> input(3 * widths[i], std::byte{0x3c}), output(input.size());
		const std::array inputs{std::span<const std::byte>(input), std::span<const std::byte>(input)};
		ProgramTestSession runtime(graphTensorSpecs, graphNodes, graphBarriers, graphOutputIndex, {});
		for (int repeat = 0; repeat < 2; ++repeat) {
			runtime.run(inputs, output);
			expect(input == output, "pass-through changed packed bytes");
		}
	}
	return 0;
}

int testPackedWeights()
{
	// Embedded weights include alignment gaps and DirectML's four-byte tensor padding.
	const TensorSpec specs[]{
		makeWeightTensorSpec(0, dtUInt8, 1, {3}),
		makeWeightTensorSpec(16, dtFloat32, 1, {1}),
	};
	const Node nodes[]{InputItem{dtUInt8, 0}, InputItem{dtFloat32, 1}};
	const std::array<std::byte, 20> weights{std::byte{1}, std::byte{2}, std::byte{3},    std::byte{0},
						std::byte{0}, std::byte{0}, std::byte{0},    std::byte{0},
						std::byte{0}, std::byte{0}, std::byte{0},    std::byte{0},
						std::byte{0}, std::byte{0}, std::byte{0},    std::byte{0},
						std::byte{0}, std::byte{0}, std::byte{0x80}, std::byte{0x3f}};
	for (std::size_t outputIndex = 0; outputIndex < 2; ++outputIndex) {
		std::span<const TensorSpec> graphTensorSpecs = specs;
		std::span<const Node> graphNodes = std::span(nodes).subspan(0, 2);
		std::span<const std::pair<std::int32_t, std::int32_t>> graphBarriers = {};
		std::int32_t graphOutputIndex = static_cast<std::int32_t>(outputIndex);
		const auto program = compile(graphTensorSpecs, graphNodes, graphBarriers, graphOutputIndex, weights);
		expect(tensorDataBytes(program.tensors[0]) == 3 && tensorSize(program.tensors[0]) == 4,
		       "weight padding is incorrect");
		expect(tensorDataBytes(program.tensors[1]) == 4 && tensorByteOffset(program.tensors[1]) == 16,
		       "weight alignment is incorrect");
		ProgramTestSession runtime(graphTensorSpecs, graphNodes, graphBarriers, graphOutputIndex, weights);
		std::vector<std::byte> output(outputIndex == 0 ? 3 : 4);
		runtime.run({}, output);
		const auto expected = std::span(weights).subspan(outputIndex == 0 ? 0 : 16, output.size());
		expect(std::equal(output.begin(), output.end(), expected.begin()), "packed weight output differs");
	}
	const std::array unalignedSpecs{specs[0], makeWeightTensorSpec(4, dtFloat32, 1, {1})};
	expectError([&] { compile(unalignedSpecs, nodes, {}, 1, weights); }, "16-byte aligned");
	expectError(
		[&] {
			compile(std::span(specs).first(1), std::span(nodes).first(1), {}, 0,
				std::span(weights).first(3));
		},
		"weight exceeds");
	return 0;
}

int testInvalidDefinitions()
{
	InputTensorSpec tensor;
	tensor.dataType = dtFloat64;
	const std::array<UINT, 2> maximumShape{65535, 65537};
	setContiguousShape(tensor, maximumShape);
	expect(tensor.dataBytes == std::uint64_t{0xffffffff} * 8, "maximum element count byte size differs");
	const std::array<UINT, 2> excessiveShape{65536, 65536};
	expectError([&] { setContiguousShape(tensor, excessiveShape); }, "overflow");

	auto spec = makeInputTensorSpec(dtFloat32, 1, {3});
	Node nodes[]{InputItem{dtFloat32, 0}};
	std::span<const TensorSpec> graphTensorSpecs = std::span(&spec, 1);
	std::span<const Node> graphNodes = std::span(nodes).subspan(0, 1);
	std::span<const std::pair<std::int32_t, std::int32_t>> graphBarriers = {};
	std::int32_t graphOutputIndex = 0;
	graphTensorSpecs = {};
	expectError([&] { compile(graphTensorSpecs, graphNodes, graphBarriers, graphOutputIndex); }, "out of range");
	graphTensorSpecs = std::span(&spec, 1);
	graphNodes = {};
	expectError([&] { compile(graphTensorSpecs, graphNodes, graphBarriers, graphOutputIndex); },
		    "invalid node table");
	graphNodes = nodes;
	const auto valid = spec;
	for (const auto offset : {-2LL, std::numeric_limits<long long>::min()}) {
		spec = makeWeightTensorSpec(offset, dtFloat32, 1, {3});
		expectError([&] { compile(graphTensorSpecs, graphNodes, graphBarriers, graphOutputIndex); },
			    "invalid input byte offset");
	}

	expectError([] { auto invalid = makeInputTensorSpec(dtUnknown, 1, {3}); }, "Unknown");
	expectError([] { auto invalid = makeInputTensorSpec(dtFloat32, 5, {3}); }, "shape rank");
	expectError([] { auto invalid = makeInputTensorSpec(dtFloat32, 1, {0}); }, "positive");
	expectError([] { auto invalid = makeInputTensorSpec(dtFloat32, 1, {-1}); }, "resolved");
	expectError(
		[] {
			auto invalid =
				makeInputTensorSpec(dtFloat32, 4, {0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff});
		},
		"overflow");
	spec = valid;
	spec = makeWeightTensorSpec(0, dtFloat32, 1, {3});
	expectError([&] { compile(graphTensorSpecs, graphNodes, graphBarriers, graphOutputIndex); }, "weight exceeds");
	spec = valid;
	nodes[0] = InputItem{dtFloat32, -1};
	expectError([&] { compile(graphTensorSpecs, graphNodes, graphBarriers, graphOutputIndex); }, "out of range");
	nodes[0] = InputItem{dtFloat32, 1};
	expectError([&] { compile(graphTensorSpecs, graphNodes, graphBarriers, graphOutputIndex); }, "out of range");
	nodes[0] = InputItem{dtFloat32, 0};
	graphOutputIndex = 1;
	expectError([&] { compile(graphTensorSpecs, graphNodes, graphBarriers, graphOutputIndex); }, "preceding node");
	const Node invalidEdges[]{InputItem{dtFloat32, 0}, Onnx14Node{Add{dtFloat32, 0, 1, 1, {3}}}};
	graphTensorSpecs = std::span(&spec, 1);
	graphNodes = std::span(invalidEdges).subspan(0, 2);
	graphBarriers = {};
	graphOutputIndex = 1;
	expectError([&] { compile(graphTensorSpecs, graphNodes, graphBarriers, graphOutputIndex); }, "preceding node");
	const TensorSpec mixedSpecs[]{spec, makeInputTensorSpec(dtUInt32, 1, {3})};
	const Node mismatch[]{InputItem{dtFloat32, 0}, InputItem{dtUInt32, 1},
			      Onnx14Node{Add{dtFloat32, 0, 1, 1, {3}}}};
	graphTensorSpecs = mixedSpecs;
	graphNodes = std::span(mismatch).subspan(0, 3);
	graphBarriers = {};
	graphOutputIndex = 2;
	expectError([&] { compile(graphTensorSpecs, graphNodes, graphBarriers, graphOutputIndex); },
		    "data types must match");
	graphTensorSpecs = std::span(&spec, 1);
	graphNodes = {};
	graphBarriers = {};
	graphOutputIndex = 1;
	const Node badUnary[]{InputItem{dtFloat32, 0}, Onnx14Node{Relu{dtFloat32, 0, 1, {2}}}};
	graphNodes = badUnary;
	expectError([&] { compile(graphTensorSpecs, graphNodes, graphBarriers, graphOutputIndex); },
		    "shapes must match");
	return 0;
}

int testIntegerArithmetic()
{
	const TensorSpec specs[]{
		makeInputTensorSpec(dtUInt32, 4, {1, 1, 1, 3}),
		makeWeightTensorSpec(0, dtUInt32, 4, {1, 1, 1, 3}),
	};
	const Node nodes[]{InputItem{dtUInt32, 0}, InputItem{dtUInt32, 1},
			   Onnx14Node{Add{dtUInt32, 0, 1, 4, {1, 1, 1, 3}}}};
	std::span<const TensorSpec> graphTensorSpecs = specs;
	std::span<const Node> graphNodes = std::span(nodes).subspan(0, 3);
	std::span<const std::pair<std::int32_t, std::int32_t>> graphBarriers = {};
	std::int32_t graphOutputIndex = 2;
	const std::array<std::uint32_t, 3> a{1, 100, 50000}, b{2, 200, 60000};
	std::unique_ptr<ProgramTestSession> runtime;
	try {
		runtime = std::make_unique<ProgramTestSession>(graphTensorSpecs, graphNodes, graphBarriers,
							       graphOutputIndex, std::as_bytes(std::span(b)));
	} catch (const std::exception &error) {
		const std::string_view message(error.what());
		if (message.find("CreateOperator (operator/type support)") == message.npos &&
		    message.find("unsupported by the DirectML device") == message.npos)
			throw;
		std::cout << "Integer Add unavailable: " << error.what() << '\n';
		return 77;
	}
	const std::array<std::span<const std::byte>, 1> inputs{std::as_bytes(std::span(a))};
	std::array<std::uint32_t, 3> output{};
	runtime->run(inputs, std::as_writable_bytes(std::span(output)));
	for (std::size_t i = 0; i < a.size(); ++i)
		expect(output[i] == a[i] + b[i], "integer Add result differs");
	return 0;
}

int testFloat16Arithmetic()
{
	const TensorSpec specs[]{
		makeInputTensorSpec(dtFloat16, 1, {3}),
		makeWeightTensorSpec(0, dtFloat16, 1, {3}),
	};
	const Node nodes[]{InputItem{dtFloat16, 0}, InputItem{dtFloat16, 1}, Onnx14Node{Add{dtFloat16, 0, 1, 1, {3}}},
			   Onnx14Node{Relu{dtFloat16, 2, 1, {3}}}};
	std::pair<std::int32_t, std::int32_t> uavBarriers[]{{3, 2}};
	std::span<const TensorSpec> graphTensorSpecs = specs;
	std::span<const Node> graphNodes = std::span(nodes).subspan(0, 4);
	std::span<const std::pair<std::int32_t, std::int32_t>> graphBarriers = uavBarriers;
	std::int32_t graphOutputIndex = 3;
	// IEEE binary16 encodings for [1, 2, 3] + 2 = [3, 4, 5].
	const std::array<std::uint16_t, 3> values{0x3c00, 0x4000, 0x4200}, expected{0x4200, 0x4400, 0x4500};
	const std::array<std::uint16_t, 4> weights{0x4000, 0x4000, 0x4000, 0};
	ProgramTestSession runtime(graphTensorSpecs, graphNodes, graphBarriers, graphOutputIndex,
				   std::as_bytes(std::span(weights)));
	const std::array<std::span<const std::byte>, 1> inputs{std::as_bytes(std::span(values))};
	std::array<std::uint16_t, 3> output{};
	runtime.run(inputs, std::as_writable_bytes(std::span(output)));
	expect(output == expected, "Float16 arithmetic or padding differs");
	return 0;
}

int testExplicitTypesAndBarriers()
{
	constexpr TensorSpec specs[]{makeInputTensorSpec(dtFloat32, 1, {4})};
	Node nodes[]{InputItem{dtFloat32, 0}, Onnx14Node{Relu{dtFloat32, 0, 1, {4}}},
		     Onnx14Node{Add{dtFloat32, 1, 1, 1, {4}}}, Onnx14Node{Mul{dtFloat32, 1, 2, 1, {4}}}};
	std::pair<std::int32_t, std::int32_t> uavBarriers[]{{2, 1}, {3, 2}};
	std::span<const TensorSpec> graphTensorSpecs = specs;
	std::span<const Node> graphNodes = std::span(nodes).subspan(0, 4);
	std::span<const std::pair<std::int32_t, std::int32_t>> graphBarriers = uavBarriers;
	std::int32_t graphOutputIndex = 3;
	const auto program = compile(graphTensorSpecs, graphNodes, graphBarriers, graphOutputIndex);
	expect(program.steps.size() == 3, "unexpected step count");
	graphOutputIndex = 0;
	expect(compile(graphTensorSpecs, graphNodes, graphBarriers, graphOutputIndex).steps.size() == 3,
	       "output selection skipped later nodes");
	graphOutputIndex = 3;
	expect(program.steps[0].barriersBefore.empty(), "unrequested input barrier");
	for (std::size_t step = 1; step < 3; ++step) {
		const auto &barriers = program.steps[step].barriersBefore;
		expect(barriers.size() == 1, "barrier table was not honored");
		expect(program.placements[barriers[0]] == program.placements[program.steps[step - 1].output],
		       "barrier targets the wrong producer storage");
	}
	// Compile only: missing synchronization must not be inferred by C++.
	graphBarriers = {};
	expect(compile(graphTensorSpecs, graphNodes, graphBarriers, graphOutputIndex).steps[1].barriersBefore.empty(),
	       "compiler inferred an unrequested barrier");
	graphBarriers = uavBarriers;
	graphBarriers = std::span(uavBarriers).subspan(1);
	expect(compile(graphTensorSpecs, graphNodes, graphBarriers, graphOutputIndex).steps[1].barriersBefore.empty(),
	       "omitted boundary ignored");
	auto &item = std::get<InputItem>(nodes[0]);
	item.dataType = dtFloat16;
	expectError([&] { compile(graphTensorSpecs, graphNodes, graphBarriers, graphOutputIndex); },
		    "InputItem and TensorSpec");
	for (const auto type : {0, -1, 99}) {
		item.dataType = type;
		expectError([&] { compile(graphTensorSpecs, graphNodes, graphBarriers, graphOutputIndex); },
			    "Unknown or invalid node data type");
	}
	item.dataType = dtFloat32;
	std::get<Add>(std::get<Onnx14Node>(nodes[2])).dataType = dtFloat16;
	expectError([&] { compile(graphTensorSpecs, graphNodes, graphBarriers, graphOutputIndex); },
		    "input and operator data types must match");
	return 0;
}

int testUavBarrierInstructions()
{
	constexpr TensorSpec specs[]{makeInputTensorSpec(dtFloat32, 1, {4})};
	const Node nodes[]{InputItem{dtFloat32, 0}, Onnx14Node{Relu{dtFloat32, 0, 1, {4}}},
			   Onnx14Node{Relu{dtFloat32, 0, 1, {4}}}, Onnx14Node{Add{dtFloat32, 1, 2, 1, {4}}}};
	std::pair<std::int32_t, std::int32_t> uavBarriers[]{{2, 1}, {3, 2}};
	std::span<const TensorSpec> graphTensorSpecs = specs;
	std::span<const Node> graphNodes = std::span(nodes).subspan(0, 4);
	std::span<const std::pair<std::int32_t, std::int32_t>> graphBarriers = uavBarriers;
	std::int32_t graphOutputIndex = 3;
	const auto program = compile(graphTensorSpecs, graphNodes, graphBarriers, graphOutputIndex);
	expect(program.steps[1].barriersBefore.size() == 1, "non-input producer instruction ignored");
	const auto first = program.steps[0].output, second = program.steps[1].output;
	expect(program.placements[program.steps[1].barriersBefore[0]] == program.placements[first],
	       "non-input producer barrier targets wrong resource");
	const std::array<float, 4> input{-2, -1, 1, 2};
	std::array<float, 4> output{};
	const std::array<std::span<const std::byte>, 1> inputs{std::as_bytes(std::span(input))};
	ProgramTestSession runtime(graphTensorSpecs, graphNodes, graphBarriers, graphOutputIndex, {});
	runtime.run(inputs, std::as_writable_bytes(std::span(output)));
	for (std::size_t i = 0; i < 4; ++i)
		expect(output[i] == 2 * std::max(0.0F, input[i]), "repositioned barrier result differs");
	// Multiple entries at one boundary retain their order and duplicates; no fixed row limit.
	const std::pair<std::int32_t, std::int32_t> multiple[]{{2, 1}, {3, 1}, {3, 2}};
	auto multipleGraphInputSpecs = graphTensorSpecs;
	auto multipleGraphNodes = graphNodes;
	auto multipleGraphBarriers = graphBarriers;
	auto multipleGraphOutputIndex = graphOutputIndex;
	multipleGraphBarriers = multiple;
	expect(compile(multipleGraphInputSpecs, multipleGraphNodes, multipleGraphBarriers, multipleGraphOutputIndex)
			       .steps[2]
			       .barriersBefore.size() == 2,
	       "multiple barrier pairs ignored");
	std::vector<std::pair<std::int32_t, std::int32_t>> repeated(12, {3, 1});
	multipleGraphBarriers = repeated;
	expect(compile(multipleGraphInputSpecs, multipleGraphNodes, multipleGraphBarriers, multipleGraphOutputIndex)
			       .steps[2]
			       .barriersBefore.size() == 12,
	       "barrier pairs or duplicates changed");
	const std::pair<std::int32_t, std::int32_t> unsorted[]{{3, 2}, {2, 1}};
	multipleGraphBarriers = unsorted;
	expectError(
		[&] {
			compile(multipleGraphInputSpecs, multipleGraphNodes, multipleGraphBarriers,
				multipleGraphOutputIndex);
		},
		"boundaries must be sorted");
	for (const auto invalid : {-2, -1, 3, 4, 999}) {
		uavBarriers[1].second = invalid;
		expectError([&] { compile(graphTensorSpecs, graphNodes, graphBarriers, graphOutputIndex); },
			    "UAV barrier must reference a preceding node");
	}
	// A barrier before a non-dispatch InputItem still occupies that boundary.
	const Node inputBoundary[]{InputItem{dtFloat32, 0}, Onnx14Node{Relu{dtFloat32, 0, 1, {4}}},
				   InputItem{dtFloat32, 0}, Onnx14Node{Add{dtFloat32, 1, 2, 1, {4}}}};
	uavBarriers[0] = {2, 1};
	const auto boundaryBarriers = std::span(uavBarriers).first(1);
	std::span<const TensorSpec> boundaryGraphInputSpecs = specs;
	std::span<const Node> boundaryGraphNodes = std::span(inputBoundary).subspan(0, 4);
	std::span<const std::pair<std::int32_t, std::int32_t>> boundaryGraphBarriers = boundaryBarriers;
	std::int32_t boundaryGraphOutputIndex = 3;
	const auto boundary =
		compile(boundaryGraphInputSpecs, boundaryGraphNodes, boundaryGraphBarriers, boundaryGraphOutputIndex);
	expect(boundary.steps.size() == 3 && boundary.steps[1].output == -1 &&
		       boundary.steps[1].barriersBefore.size() == 1,
	       "InputItem boundary lost");
	ProgramTestSession boundaryRuntime(boundaryGraphInputSpecs, boundaryGraphNodes, boundaryGraphBarriers,
					   boundaryGraphOutputIndex, {});
	const std::array boundaryInputs{inputs[0], inputs[0]};
	boundaryRuntime.run(boundaryInputs, std::as_writable_bytes(std::span(output)));
	for (std::size_t i = 0; i < 4; ++i)
		expect(output[i] == std::max(0.0F, input[i]) + input[i], "InputItem boundary result differs");
	std::span<const TensorSpec> trailingInputSpecs = specs;
	std::span<const Node> trailingNodes = std::span(inputBoundary).subspan(0, 3);
	std::span<const std::pair<std::int32_t, std::int32_t>> trailingBarriers = boundaryBarriers;
	std::int32_t trailingOutputIndex = 2;
	ProgramTestSession trailingRuntime(trailingInputSpecs, trailingNodes, trailingBarriers, trailingOutputIndex,
					   {});
	trailingRuntime.run(boundaryInputs, std::as_writable_bytes(std::span(output)));
	expect(output == input, "trailing barrier-only step changed pass-through output");
	return 0;
}

int testUavBarrierWeightResource()
{
	const TensorSpec specs[]{makeInputTensorSpec(dtFloat32, 1, {4}), makeWeightTensorSpec(0, dtFloat32, 1, {4})};
	const Node nodes[]{InputItem{dtFloat32, 0}, InputItem{dtFloat32, 1}, Onnx14Node{Add{dtFloat32, 0, 1, 1, {4}}}};
	const std::pair<std::int32_t, std::int32_t> uavBarriers[]{{2, 1}};
	const std::array<float, 4> weights{1, 2, 3, 4}, input{4, 3, 2, 1};
	std::array<float, 4> output{};
	ProgramTestSession runtime(specs, std::span(nodes).subspan(0, 3), uavBarriers, 2,
				   std::as_bytes(std::span(weights)));
	const std::array<std::span<const std::byte>, 1> inputs{std::as_bytes(std::span(input))};
	runtime.run(inputs, std::as_writable_bytes(std::span(output)));
	for (const auto value : output)
		expect(value == 5, "weight barrier result differs");
	return 0;
}

// The caller owns submission and waits; recording is deliberately outside the replay loop.
auto exerciseRecordedExecution(bool combinedInitialization) -> int
{
	ComPtr<ID3D12Device> d3d;
	DmlProgramTest::check(device->GetParentDevice(IID_PPV_ARGS(&d3d)), "Get test parent device");
	const TensorSpec specs[]{makeInputTensorSpec(dtFloat32, 1, {1}), makeWeightTensorSpec(0, dtFloat32, 1, {1})};
	const Node nodes[]{InputItem{dtFloat32, 0}, InputItem{dtFloat32, 1}, Onnx14Node{Add{dtFloat32, 0, 1, 1, {1}}}};
	const float weight = 2;
	const auto tables = makeModelParamsStorage(std::span(specs), std::span(nodes), {}, 2, sizeof(weight));
	const auto definition = makeDmlProgramDefinition(nodes, tables.params(), std::as_bytes(std::span(&weight, 1)));
	auto original = createDmlProgram(d3d.Get(), device.Get(), definition);
	auto *const storage = original.storage(0);
	auto *const recorder = original.commandRecorder();
	DmlProgram program;
	program = std::move(original);
	expect(program.storage(0) == storage && program.commandRecorder() == recorder,
	       "moving the program must preserve resource addresses");
	expect(original.commandRecorder() == nullptr, "moving must transfer recorder ownership");
	expect(program.internalTensor(encodeInternalTensorId(0, 0)) == nullptr,
	       "absent internal tensor must not allocate a resource");
	ComPtr<ID3D12CommandQueue> queue;
	D3D12_COMMAND_QUEUE_DESC desc{};
	desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	DmlProgramTest::check(d3d->CreateCommandQueue(&desc, IID_PPV_ARGS(&queue)), "Create test queue");
	ComPtr<ID3D12CommandAllocator> allocator;
	DmlProgramTest::check(d3d->CreateCommandAllocator(desc.Type, IID_PPV_ARGS(&allocator)),
			      "Create test allocator");
	ComPtr<ID3D12GraphicsCommandList> commands;
	DmlProgramTest::check(d3d->CreateCommandList(0, desc.Type, allocator.Get(), nullptr, IID_PPV_ARGS(&commands)),
			      "Create test commands");
	ComPtr<ID3D12Fence> fence;
	DmlProgramTest::check(d3d->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)), "Create test fence");
	// Nonzero offsets exercise the recorded copy arguments as well as replay.
	constexpr std::uint64_t inputOffset = 4, outputOffset = 8;
	auto upload = createBuffer(d3d.Get(), inputOffset + sizeof(float), D3D12_HEAP_TYPE_UPLOAD,
				   D3D12_RESOURCE_STATE_GENERIC_READ);
	auto output = createBuffer(d3d.Get(), outputOffset + sizeof(float), D3D12_HEAP_TYPE_READBACK,
				   D3D12_RESOURCE_STATE_COPY_DEST);
	std::uint64_t fenceValue = 0;
	auto submitAndWait = [&] {
		ID3D12CommandList *lists[]{commands.Get()};
		queue->ExecuteCommandLists(1, lists);
		const auto value = ++fenceValue;
		const HRESULT signal = queue->Signal(fence.Get(), value);
		if (FAILED(signal) && FAILED(queue->Signal(fence.Get(), value))) {
			if (SUCCEEDED(d3d->GetDeviceRemovedReason()))
				std::terminate();
			DmlProgramTest::check(signal, "Signal completion");
		}
		for (;;) {
			const auto completed = fence->GetCompletedValue();
			if (completed == UINT64_MAX)
				throw std::runtime_error("Test device removed");
			if (completed >= value)
				break;
			DmlProgramTest::check(d3d->GetDeviceRemovedReason(), "Wait for completion");
			Sleep(1);
		}
		DmlProgramTest::check(signal, "Signal completion");
	};
	static_assert(noexcept(recordInitialization(definition, program, commands.Get())));
	static_assert(noexcept(inputInfo(definition, 0)));
	recordInitialization(definition, program, commands.Get());
	if (!combinedInitialization) {
		DmlProgramTest::check(commands->Close(), "Close initialization commands");
		submitAndWait();
		DmlProgramTest::check(allocator->Reset(), "Reset initialization allocator");
		DmlProgramTest::check(commands->Reset(allocator.Get(), nullptr), "Reset initialization commands");
	}
	const std::array inputs{GpuInput{upload.get(), inputOffset, sizeof(float)}};
	static_assert(noexcept(
		recordGpuRun(definition, program, commands.Get(), inputs, nullptr, output.get(), outputOffset)));
	recordGpuRun(definition, program, commands.Get(), inputs, nullptr, output.get(), outputOffset);
	DmlProgramTest::check(commands->Close(), "Close execution commands");
	const std::array values{3.0F, 7.0F, -5.0F};
	for (const float input : std::span(values).first(combinedInitialization ? 1 : values.size())) {
		void *mapped = nullptr;
		const D3D12_RANGE noRead{0, 0};
		DmlProgramTest::check(upload->Map(0, &noRead, &mapped), "Map input");
		std::memcpy(static_cast<std::byte *>(mapped) + inputOffset, &input, sizeof(input));
		upload->Unmap(0, nullptr);
		submitAndWait(); // No Reset(), re-recording or repeated initialization.
		const D3D12_RANGE range{outputOffset, outputOffset + sizeof(float)};
		DmlProgramTest::check(output->Map(0, &range, &mapped), "Map output");
		float result = 0;
		std::memcpy(&result, static_cast<const std::byte *>(mapped) + outputOffset, sizeof(result));
		const D3D12_RANGE noWrite{0, 0};
		output->Unmap(0, &noWrite);
		expect(result == input + weight, "recorded execution did not use the latest input");
	}
	return 0;
}

int testInitializationAndExecutionInOneSubmission()
{
	return exerciseRecordedExecution(true);
}

int testExecutionOnlyCommandListReplay()
{
	return exerciseRecordedExecution(false);
}

int testCompileErrorsAreExceptions()
{
	checkDmlProgramHeaderIsolation();
	const auto valid = makeInputTensorSpec(dtFloat32, 1, {1});
	const std::array specs{valid};
	const Node input[]{InputItem{dtFloat32, 0}};
	auto successful = makeCompiler(device.Get(), specs, input, {}, 0, {});
	const auto result = successful.takeSteps();
	expect(result.empty(), "an input-only program must not allocate operator resources");
	try {
		auto invalidDevice = makeCompiler(nullptr, specs, input, {}, 0, {});
		throw std::runtime_error("null device succeeded");
	} catch (const std::invalid_argument &error) {
		expect(std::string_view(error.what()) == "device is null", "invalid argument message differs");
	}

	try {
		auto invalidInput = makeInputTensorSpec(dtFloat32, 1, {0});
		throw std::runtime_error("invalid input definition succeeded");
	} catch (const DmlCompilationError &error) {
		expect(!error.tensorSpecIndex && !error.nodeIndex && !error.dataType,
		       "input compile error location/type differs");
		expect(std::string_view(error.what()) == "shape dimensions must be positive and resolved",
		       "input compile diagnostic differs");
	}

	const Node badOperand[]{InputItem{dtFloat32, 0}, Onnx14Node{Relu{dtFloat32, 1, 1, {1}}}};
	try {
		auto invalidReference = makeCompiler(device.Get(), specs, badOperand, {}, 1, {});
		throw std::runtime_error("invalid operand succeeded");
	} catch (const DmlCompilationError &error) {
		expect(error.nodeIndex == 1 && !error.tensorSpecIndex && !error.dataType,
		       "operand error lost its context");
		expect(std::string_view(error.what()) == "input must reference a preceding node",
		       "operand diagnostic differs");
	}

	const Node badClip[]{InputItem{dtFloat32, 0}, Onnx13Node{ClipFloat32{0, 1, {1}, 1.0F, 0.0F}}};
	try {
		auto invalidOperator = makeCompiler(device.Get(), specs, badClip, {}, 1, {});
		throw std::runtime_error("invalid operator succeeded");
	} catch (const DmlCompilationError &error) {
		expect(error.nodeIndex == 1 && !error.tensorSpecIndex && !error.dataType,
		       "operator error location/type differs");
		expect(std::string_view(error.what()) == "ClipFloat32 minimum exceeds maximum",
		       "what() did not preserve its fixed string");
	}
	const auto typeError = Errors::inputDataTypeMismatch(2, dtFloat16);
	const DmlCompilationError &typedError = typeError;
	expect(typedError.nodeIndex == 2 && !typedError.tensorSpecIndex && typedError.dataType == dtFloat16,
	       "typed error optional context differs");
	const auto featureError = Errors::featureSupportError(E_INVALIDARG, std::nullopt, 3, dtFloat64);
	expect(!featureError.nodeIndex && featureError.tensorSpecIndex == 3 && featureError.dataType == dtFloat64 &&
		       featureError.result == E_INVALIDARG,
	       "input API error optional context differs");
	const auto apiError = Errors::operatorCompilationError(E_INVALIDARG, 4);
	expect(apiError.result == E_INVALIDARG && apiError.nodeIndex == 4 && !apiError.tensorSpecIndex &&
		       !apiError.dataType,
	       "DirectML error lost its HRESULT");
	expect(std::string_view(apiError.what()) == "CompileOperator failed", "DirectML diagnostic differs");

	// Construction preserves the compilation diagnostic without runtime_error conversion.
	ComPtr<ID3D12Device> owner;
	expect(SUCCEEDED(device->GetParentDevice(IID_PPV_ARGS(&owner))), "GetParentDevice failed");
	try {
		const auto tables = makeModelParamsStorage(std::span(specs), std::span(badClip), {}, 1, 0);
		auto program = createDmlProgram(owner.Get(), device.Get(),
						makeDmlProgramDefinition(badClip, tables.params(), {}));
		throw std::runtime_error("invalid program construction succeeded");
	} catch (const DmlCompilationError &error) {
		expect(error.nodeIndex == 1 && std::string_view(error.what()) == "ClipFloat32 minimum exceeds maximum",
		       "construction lost the compile diagnostic");
	}
	return 0;
}

int testMain(int argc, char **argv)
{
	ComPtr<ID3D12Device> d3d;
	const auto d3dResult = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d));
	const auto dmlResult = SUCCEEDED(d3dResult)
				       ? DMLCreateDevice(d3d.Get(), DML_CREATE_DEVICE_FLAG_NONE, IID_PPV_ARGS(&device))
				       : d3dResult;
	if (FAILED(dmlResult)) {
		std::cout << "DirectML device unavailable (HRESULT " << dmlResult << ")\n";
		return 77;
	}
	try {
		expect(argc == 2, "Expected test case name");
		const std::string_view name(argv[1]);
		if (name == "testExplicitShapeNodes")
			return testExplicitShapeNodes();
		if (name == "testCompileErrorsAreExceptions")
			return testCompileErrorsAreExceptions();
		if (name == "testExecutionOnlyCommandListReplay")
			return testExecutionOnlyCommandListReplay();
		if (name == "testInitializationAndExecutionInOneSubmission")
			return testInitializationAndExecutionInOneSubmission();
		if (name == "testUavBarrierWeightResource")
			return testUavBarrierWeightResource();
		if (name == "testUavBarrierInstructions")
			return testUavBarrierInstructions();
		if (name == "testExplicitTypesAndBarriers")
			return testExplicitTypesAndBarriers();
		if (name == "testMultipleInputsAndReuse")
			return testMultipleInputsAndReuse();
		if (name == "testTypedPaddingAndPassthrough")
			return testTypedPaddingAndPassthrough();
		if (name == "testPackedWeights")
			return testPackedWeights();
		if (name == "testInvalidDefinitions")
			return testInvalidDefinitions();
		if (name == "testIntegerArithmetic")
			return testIntegerArithmetic();
		if (name == "testFloat16Arithmetic")
			return testFloat16Arithmetic();
		throw std::runtime_error("Unknown test case");
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}

int main(int argc, char **argv)
{
	if (TraceLoggingRegister(g_hProvider) != ERROR_SUCCESS)
		return 1;
	try {
		const int result = testMain(argc, argv);
		TraceLoggingUnregister(g_hProvider);
		return result;
	} catch (...) {
		TraceLoggingUnregister(g_hProvider);
		throw;
	}
}
