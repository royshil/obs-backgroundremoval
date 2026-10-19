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

#include <DmlProgram/BaselineDmlCompiler.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace BaselineDml;
using namespace BaselineDml::ProgramDefinition;
using Microsoft::WRL::ComPtr;

namespace {
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

auto compile(std::span<const InputSpecItem> inputSpecs, std::span<const Node> graphNodes,
	     std::span<const std::pair<std::int32_t, std::int32_t>> uavBarriers, std::int32_t outputIndex,
	     std::span<const std::byte> weights = {}) -> CompiledDmlProgram
{
	return BaselineDmlCompiler(device.Get()).compile(inputSpecs, graphNodes, uavBarriers, outputIndex, weights);
}
} // namespace

int testMultipleInputsAndReuse()
{
	static_assert(sizeof(InputSpecItem) == 32);
	const std::array specs{
		InputSpecItem{kRuntimeInput, dtFloat32, 4, {1, 1, 1, 3}},
		InputSpecItem{0, dtFloat32, 4, {1, 1, 1, 3}},
		InputSpecItem{kRuntimeInput, dtFloat32, 4, {1, 1, 1, 3}},
	};
	const Node nodes[]{InputItem{dtFloat32, 2},
			   InputItem{dtFloat32, 1},
			   InputItem{dtFloat32, 0},
			   InputItem{dtFloat32, 0},
			   Onnx14Node{Mul{dtFloat32, 2, 1, 4, {1, 1, 1, 3}}},
			   Onnx14Node{Add{dtFloat32, 4, 0, 4, {1, 1, 1, 3}}},
			   Onnx14Node{Add{dtFloat32, 5, 3, 4, {1, 1, 1, 3}}}};
	std::pair<std::int32_t, std::int32_t> uavBarriers[]{{5, 4}, {6, 5}};
	std::span<const InputSpecItem> graphInputSpecs = specs;
	std::span<const Node> graphNodes = std::span(nodes).subspan(0, static_cast<std::int32_t>(std::size(nodes)));
	std::span<const std::pair<std::int32_t, std::int32_t>> graphBarriers = uavBarriers;
	std::int32_t graphOutputIndex = 6;
	const std::array<float, 3> weights{2, 2, 2};
	const auto weightBytes = std::as_bytes(std::span(weights));
	const auto program = compile(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex, weightBytes);
	expect(program.inputTensors.size() == 2, "runtime input count includes weights");
	const auto &reused = program.tensors.at(program.steps.back().inputs[1]);
	expect(reused.storage == program.tensors.at(program.inputTensors[0]).storage,
	       "repeated input creates new storage");
	BaselineDmlProgramRuntime runtime(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex, weightBytes);
	for (int iteration = 1; iteration <= 2; ++iteration) {
		const std::array<float, 3> a{float(iteration), 2, 3}, b{10, 20, 30};
		std::array<float, 3> output{};
		const std::array<std::span<const std::byte>, 2> inputs{std::as_bytes(std::span(a)),
								       std::as_bytes(std::span(b))};
		runtime.run(inputs, std::as_writable_bytes(std::span(output)));
		for (std::size_t i = 0; i < output.size(); ++i)
			expect(std::abs(output[i] - (3 * a[i] + b[i])) < 1e-5F, "multiple input arithmetic differs");
		expectError([&] { runtime.run(std::span(inputs).first(1), std::as_writable_bytes(std::span(output))); },
			    "input count");
		const std::array wrongInputs{inputs[0], inputs[1].first(4)};
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
		const InputSpecItem spec{kRuntimeInput, types[i], 1, {3}};
		const Node nodes[]{InputItem{static_cast<std::int32_t>(types[i]), 0},
				   InputItem{static_cast<std::int32_t>(types[i]), 0}};
		std::span<const InputSpecItem> graphInputSpecs = std::span(&spec, 1);
		std::span<const Node> graphNodes = std::span(nodes).subspan(0, 2);
		std::span<const std::pair<std::int32_t, std::int32_t>> graphBarriers = {};
		std::int32_t graphOutputIndex = 1;
		DML_FEATURE_QUERY_TENSOR_DATA_TYPE_SUPPORT query{static_cast<DML_TENSOR_DATA_TYPE>(types[i])};
		DML_FEATURE_DATA_TENSOR_DATA_TYPE_SUPPORT support{};
		expect(SUCCEEDED(device->CheckFeatureSupport(DML_FEATURE_TENSOR_DATA_TYPE_SUPPORT, sizeof(query),
							     &query, sizeof(support), &support)),
		       "type support query failed");
		if (!support.IsSupported) {
			expectError([&] { compile(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex); },
				    "unsupported by the DirectML device");
			std::cout << "Device rejects data type " << static_cast<unsigned>(types[i]) << '\n';
			continue;
		}
		const auto program = compile(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex);
		expect(program.tensors.size() == 1 && program.storages.size() == 1,
		       "input specification was not reused");
		expect(program.tensors[0].dataBytes == 3 * widths[i], "incorrect element width");
		expect(program.tensors[0].size == (3 * widths[i] + 3) / 4 * 4, "incorrect buffer padding");
		std::vector<std::byte> input(3 * widths[i], std::byte{0x3c}), output(input.size());
		const std::array inputs{std::span<const std::byte>(input)};
		BaselineDmlProgramRuntime runtime(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex, {});
		for (int repeat = 0; repeat < 2; ++repeat) {
			runtime.run(inputs, output);
			expect(input == output, "pass-through changed packed bytes");
		}
	}
	return 0;
}

int testPackedWeights()
{
	// Odd byte counts and an unaligned source offset must not read GPU padding from the resource.
	const InputSpecItem specs[]{
		{0, dtUInt8, 1, {3}},
		{3, dtFloat32, 1, {1}},
	};
	const Node nodes[]{InputItem{dtUInt8, 0}, InputItem{dtFloat32, 1}};
	const std::array<std::byte, 7> weights{std::byte{1}, std::byte{2},    std::byte{3},   std::byte{0},
					       std::byte{0}, std::byte{0x80}, std::byte{0x3f}};
	for (std::size_t outputIndex = 0; outputIndex < 2; ++outputIndex) {
		std::span<const InputSpecItem> graphInputSpecs = specs;
		std::span<const Node> graphNodes = std::span(nodes).subspan(0, 2);
		std::span<const std::pair<std::int32_t, std::int32_t>> graphBarriers = {};
		std::int32_t graphOutputIndex = static_cast<std::int32_t>(outputIndex);
		const auto program = compile(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex, weights);
		expect(program.tensors[0].dataBytes == 3 && program.tensors[0].size == 4,
		       "weight padding is incorrect");
		expect(program.tensors[1].dataBytes == 4 && program.tensors[1].weightOffset == 16,
		       "weight alignment is incorrect");
		BaselineDmlProgramRuntime runtime(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex,
						  weights);
		std::vector<std::byte> output(outputIndex == 0 ? 3 : 4);
		runtime.run({}, output);
		const auto expected = std::span(weights).subspan(outputIndex == 0 ? 0 : 3, output.size());
		expect(std::equal(output.begin(), output.end(), expected.begin()), "packed weight output differs");
	}
	return 0;
}

int testInvalidDefinitions()
{
	InputSpecItem spec{kRuntimeInput, dtFloat32, 1, {3}};
	Node nodes[]{InputItem{dtFloat32, 0}};
	std::span<const InputSpecItem> graphInputSpecs = std::span(&spec, 1);
	std::span<const Node> graphNodes = std::span(nodes).subspan(0, 1);
	std::span<const std::pair<std::int32_t, std::int32_t>> graphBarriers = {};
	std::int32_t graphOutputIndex = 0;
	graphInputSpecs = {};
	expectError([&] { compile(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex); }, "out of range");
	graphInputSpecs = std::span(&spec, 1);
	graphNodes = {};
	expectError([&] { compile(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex); },
		    "invalid node table");
	graphNodes = nodes;
	const auto valid = spec;
	for (const auto offset : {-2LL, std::numeric_limits<long long>::min()}) {
		spec.byteOffset = offset;
		expectError([&] { compile(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex); },
			    "invalid input byte offset");
	}
	spec = valid;
	spec.dataType = dtUnknown;
	expectError([&] { compile(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex); }, "Unknown");
	spec = valid;
	spec.shapeRank = 5;
	expectError([&] { compile(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex); }, "shape rank");
	spec = valid;
	spec.shape[0] = 0;
	expectError([&] { compile(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex); }, "positive");
	spec.shape[0] = -1;
	expectError([&] { compile(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex); }, "resolved");
	spec = valid;
	spec.shapeRank = 4;
	spec.shape = {0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff};
	expectError([&] { compile(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex); }, "overflow");
	spec = valid;
	spec.byteOffset = 0;
	expectError([&] { compile(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex); }, "weight exceeds");
	spec = valid;
	nodes[0] = InputItem{dtFloat32, -1};
	expectError([&] { compile(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex); }, "node 0");
	nodes[0] = InputItem{dtFloat32, 1};
	expectError([&] { compile(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex); }, "out of range");
	nodes[0] = InputItem{dtFloat32, 0};
	graphOutputIndex = 1;
	expectError([&] { compile(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex); }, "preceding node");
	const Node invalidEdges[]{InputItem{dtFloat32, 0}, Onnx14Node{Add{dtFloat32, 0, 1, 1, {3}}}};
	graphInputSpecs = std::span(&spec, 1);
	graphNodes = std::span(invalidEdges).subspan(0, 2);
	graphBarriers = {};
	graphOutputIndex = 1;
	expectError([&] { compile(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex); }, "node 1");
	const InputSpecItem mixedSpecs[]{spec, {kRuntimeInput, dtUInt32, 1, {3}}};
	const Node mismatch[]{InputItem{dtFloat32, 0}, InputItem{dtUInt32, 1},
			      Onnx14Node{Add{dtFloat32, 0, 1, 1, {3}}}};
	graphInputSpecs = mixedSpecs;
	graphNodes = std::span(mismatch).subspan(0, 3);
	graphBarriers = {};
	graphOutputIndex = 2;
	expectError([&] { compile(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex); },
		    "data types must match");
	graphInputSpecs = std::span(&spec, 1);
	graphNodes = {};
	graphBarriers = {};
	graphOutputIndex = 1;
	const Node badUnary[]{InputItem{dtFloat32, 0}, Onnx14Node{Relu{dtFloat32, 0, 1, {2}}}};
	graphNodes = badUnary;
	expectError([&] { compile(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex); }, "element count");
	return 0;
}

int testIntegerArithmetic()
{
	const InputSpecItem specs[]{
		{kRuntimeInput, dtUInt32, 4, {1, 1, 1, 3}},
		{0, dtUInt32, 4, {1, 1, 1, 3}},
	};
	const Node nodes[]{InputItem{dtUInt32, 0}, InputItem{dtUInt32, 1},
			   Onnx14Node{Add{dtUInt32, 0, 1, 4, {1, 1, 1, 3}}}};
	std::span<const InputSpecItem> graphInputSpecs = specs;
	std::span<const Node> graphNodes = std::span(nodes).subspan(0, 3);
	std::span<const std::pair<std::int32_t, std::int32_t>> graphBarriers = {};
	std::int32_t graphOutputIndex = 2;
	const std::array<std::uint32_t, 3> a{1, 100, 50000}, b{2, 200, 60000};
	std::unique_ptr<BaselineDmlProgramRuntime> runtime;
	try {
		runtime = std::make_unique<BaselineDmlProgramRuntime>(graphInputSpecs, graphNodes, graphBarriers,
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
	const InputSpecItem specs[]{
		{kRuntimeInput, dtFloat16, 1, {3}},
		{0, dtFloat16, 1, {1}},
	};
	const Node nodes[]{InputItem{dtFloat16, 0}, InputItem{dtFloat16, 1}, Onnx14Node{Add{dtFloat16, 0, 1, 1, {3}}},
			   Onnx14Node{Relu{dtFloat16, 2, 1, {3}}}};
	std::pair<std::int32_t, std::int32_t> uavBarriers[]{{3, 2}};
	std::span<const InputSpecItem> graphInputSpecs = specs;
	std::span<const Node> graphNodes = std::span(nodes).subspan(0, 4);
	std::span<const std::pair<std::int32_t, std::int32_t>> graphBarriers = uavBarriers;
	std::int32_t graphOutputIndex = 3;
	// IEEE binary16 encodings for [1, 2, 3] + 2 = [3, 4, 5].
	const std::array<std::uint16_t, 3> values{0x3c00, 0x4000, 0x4200}, expected{0x4200, 0x4400, 0x4500};
	const std::array<std::uint16_t, 1> weights{0x4000};
	BaselineDmlProgramRuntime runtime(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex,
					  std::as_bytes(std::span(weights)));
	const std::array<std::span<const std::byte>, 1> inputs{std::as_bytes(std::span(values))};
	std::array<std::uint16_t, 3> output{};
	runtime.run(inputs, std::as_writable_bytes(std::span(output)));
	expect(output == expected, "Float16 arithmetic or padding differs");
	return 0;
}

int testExplicitTypesAndBarriers()
{
	constexpr InputSpecItem specs[]{{kRuntimeInput, dtFloat32, 1, {4}}};
	Node nodes[]{InputItem{dtFloat32, 0}, Onnx14Node{Relu{dtFloat32, 0, 1, {4}}},
		     Onnx14Node{Add{dtFloat32, 1, 1, 1, {4}}}, Onnx14Node{Mul{dtFloat32, 1, 2, 1, {4}}}};
	std::pair<std::int32_t, std::int32_t> uavBarriers[]{{2, 1}, {3, 2}};
	std::span<const InputSpecItem> graphInputSpecs = specs;
	std::span<const Node> graphNodes = std::span(nodes).subspan(0, 4);
	std::span<const std::pair<std::int32_t, std::int32_t>> graphBarriers = uavBarriers;
	std::int32_t graphOutputIndex = 3;
	const auto program = compile(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex);
	expect(program.steps.size() == 3, "unexpected step count");
	graphOutputIndex = 0;
	expect(compile(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex).steps.size() == 3,
	       "output selection skipped later nodes");
	graphOutputIndex = 3;
	expect(program.steps[0].barriersBefore.empty(), "unrequested input barrier");
	for (std::size_t step = 1; step < 3; ++step) {
		const auto &barriers = program.steps[step].barriersBefore;
		expect(barriers.size() == 1, "barrier table was not honored");
		expect(program.tensors[barriers[0]].storage == program.tensors[program.steps[step - 1].output].storage,
		       "barrier targets the wrong producer storage");
	}
	// Compile only: missing synchronization must not be inferred by C++.
	graphBarriers = {};
	expect(compile(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex).steps[1].barriersBefore.empty(),
	       "compiler inferred an unrequested barrier");
	graphBarriers = uavBarriers;
	graphBarriers = std::span(uavBarriers).subspan(1);
	expect(compile(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex).steps[1].barriersBefore.empty(),
	       "omitted boundary ignored");
	auto &item = std::get<InputItem>(nodes[0]);
	item.dataType = dtFloat16;
	expectError([&] { compile(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex); },
		    "InputItem and InputSpec");
	for (const auto type : {0, -1, 99}) {
		item.dataType = type;
		expectError([&] { compile(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex); },
			    "node 0 InputItem");
	}
	item.dataType = dtFloat32;
	std::get<Add>(std::get<Onnx14Node>(nodes[2])).dataType = dtFloat16;
	expectError([&] { compile(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex); },
		    "node 2 Add (Float16)");
	return 0;
}

int testUavBarrierInstructions()
{
	constexpr InputSpecItem specs[]{{kRuntimeInput, dtFloat32, 1, {4}}};
	const Node nodes[]{InputItem{dtFloat32, 0}, Onnx14Node{Relu{dtFloat32, 0, 1, {4}}},
			   Onnx14Node{Relu{dtFloat32, 0, 1, {4}}}, Onnx14Node{Add{dtFloat32, 1, 2, 1, {4}}}};
	std::pair<std::int32_t, std::int32_t> uavBarriers[]{{2, 1}, {3, 2}};
	std::span<const InputSpecItem> graphInputSpecs = specs;
	std::span<const Node> graphNodes = std::span(nodes).subspan(0, 4);
	std::span<const std::pair<std::int32_t, std::int32_t>> graphBarriers = uavBarriers;
	std::int32_t graphOutputIndex = 3;
	const auto program = compile(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex);
	expect(program.steps[1].barriersBefore.size() == 1, "non-input producer instruction ignored");
	const auto first = program.steps[0].output, second = program.steps[1].output;
	expect(program.tensors[program.steps[1].barriersBefore[0]].storage == program.tensors[first].storage,
	       "non-input producer barrier targets wrong resource");
	const std::array<float, 4> input{-2, -1, 1, 2};
	std::array<float, 4> output{};
	const std::array<std::span<const std::byte>, 1> inputs{std::as_bytes(std::span(input))};
	BaselineDmlProgramRuntime runtime(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex, {});
	runtime.run(inputs, std::as_writable_bytes(std::span(output)));
	for (std::size_t i = 0; i < 4; ++i)
		expect(output[i] == 2 * std::max(0.0F, input[i]), "repositioned barrier result differs");
	// Multiple entries at one boundary retain their order and duplicates; no fixed row limit.
	const std::pair<std::int32_t, std::int32_t> multiple[]{{2, 1}, {3, 1}, {3, 2}};
	auto multipleGraphInputSpecs = graphInputSpecs;
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
		expectError([&] { compile(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex); },
			    "UAV barrier must reference a preceding node");
	}
	// A barrier before a non-dispatch InputItem still occupies that boundary.
	const Node inputBoundary[]{InputItem{dtFloat32, 0}, Onnx14Node{Relu{dtFloat32, 0, 1, {4}}},
				   InputItem{dtFloat32, 0}, Onnx14Node{Add{dtFloat32, 1, 2, 1, {4}}}};
	uavBarriers[0] = {2, 1};
	const auto boundaryBarriers = std::span(uavBarriers).first(1);
	std::span<const InputSpecItem> boundaryGraphInputSpecs = specs;
	std::span<const Node> boundaryGraphNodes = std::span(inputBoundary).subspan(0, 4);
	std::span<const std::pair<std::int32_t, std::int32_t>> boundaryGraphBarriers = boundaryBarriers;
	std::int32_t boundaryGraphOutputIndex = 3;
	const auto boundary =
		compile(boundaryGraphInputSpecs, boundaryGraphNodes, boundaryGraphBarriers, boundaryGraphOutputIndex);
	expect(boundary.steps.size() == 3 && boundary.steps[1].output == -1 &&
		       boundary.steps[1].barriersBefore.size() == 1,
	       "InputItem boundary lost");
	BaselineDmlProgramRuntime boundaryRuntime(boundaryGraphInputSpecs, boundaryGraphNodes, boundaryGraphBarriers,
						  boundaryGraphOutputIndex, {});
	boundaryRuntime.run(inputs, std::as_writable_bytes(std::span(output)));
	for (std::size_t i = 0; i < 4; ++i)
		expect(output[i] == std::max(0.0F, input[i]) + input[i], "InputItem boundary result differs");
	std::span<const InputSpecItem> trailingInputSpecs = specs;
	std::span<const Node> trailingNodes = std::span(inputBoundary).subspan(0, 3);
	std::span<const std::pair<std::int32_t, std::int32_t>> trailingBarriers = boundaryBarriers;
	std::int32_t trailingOutputIndex = 2;
	BaselineDmlProgramRuntime trailingRuntime(trailingInputSpecs, trailingNodes, trailingBarriers,
						  trailingOutputIndex, {});
	trailingRuntime.run(inputs, std::as_writable_bytes(std::span(output)));
	expect(output == input, "trailing barrier-only step changed pass-through output");
	return 0;
}

int testUavBarrierWeightResource()
{
	const InputSpecItem specs[]{{kRuntimeInput, dtFloat32, 1, {4}}, {0, dtFloat32, 1, {4}}};
	const Node nodes[]{InputItem{dtFloat32, 0}, InputItem{dtFloat32, 1}, Onnx14Node{Add{dtFloat32, 0, 1, 1, {4}}}};
	const std::pair<std::int32_t, std::int32_t> uavBarriers[]{{2, 1}};
	const std::array<float, 4> weights{1, 2, 3, 4}, input{4, 3, 2, 1};
	std::array<float, 4> output{};
	BaselineDmlProgramRuntime runtime(specs, std::span(nodes).subspan(0, 3), uavBarriers, 2,
					  std::as_bytes(std::span(weights)));
	const std::array<std::span<const std::byte>, 1> inputs{std::as_bytes(std::span(input))};
	runtime.run(inputs, std::as_writable_bytes(std::span(output)));
	for (const auto value : output)
		expect(value == 5, "weight barrier result differs");
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
