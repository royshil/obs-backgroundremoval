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
// SPDX-License-Identifier: GPL-3.0-or-later

#include "DmlProgram/BaselineDmlCompiler.hpp"

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

auto compile(const Graph &graph, std::span<const std::byte> weights = {}) -> CompiledDmlProgram
{
	return BaselineDmlCompiler(device.Get()).compile(graph, weights);
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
	std::array<std::int32_t, 8> uavBarriers[7];
	for (auto &row : uavBarriers)
		row.fill(-1);
	uavBarriers[5][0] = 4;
	uavBarriers[6][0] = 5;
	const Graph graph{specs.size(), specs.data(), std::size(nodes), nodes, 6, uavBarriers};
	const std::array<float, 3> weights{2, 2, 2};
	const auto weightBytes = std::as_bytes(std::span(weights));
	const auto program = compile(graph, weightBytes);
	expect(program.inputTensors.size() == 2, "runtime input count includes weights");
	const auto &reused = program.tensors.at(program.steps.back().inputs[1]);
	expect(reused.storage == program.tensors.at(program.inputTensors[0]).storage,
	       "repeated input creates new storage");
	BaselineDmlProgramRuntime runtime(graph, weightBytes);
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
		const Graph graph{1, &spec, 2, nodes, 1};
		DML_FEATURE_QUERY_TENSOR_DATA_TYPE_SUPPORT query{static_cast<DML_TENSOR_DATA_TYPE>(types[i])};
		DML_FEATURE_DATA_TENSOR_DATA_TYPE_SUPPORT support{};
		expect(SUCCEEDED(device->CheckFeatureSupport(DML_FEATURE_TENSOR_DATA_TYPE_SUPPORT, sizeof(query),
							     &query, sizeof(support), &support)),
		       "type support query failed");
		if (!support.IsSupported) {
			expectError([&] { compile(graph); }, "unsupported by the DirectML device");
			std::cout << "Device rejects data type " << static_cast<unsigned>(types[i]) << '\n';
			continue;
		}
		const auto program = compile(graph);
		expect(program.tensors.size() == 1 && program.storages.size() == 1,
		       "input specification was not reused");
		expect(program.tensors[0].dataBytes == 3 * widths[i], "incorrect element width");
		expect(program.tensors[0].size == (3 * widths[i] + 3) / 4 * 4, "incorrect buffer padding");
		std::vector<std::byte> input(3 * widths[i], std::byte{0x3c}), output(input.size());
		const std::array inputs{std::span<const std::byte>(input)};
		BaselineDmlProgramRuntime runtime(graph, {});
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
		const Graph graph{2, specs, 2, nodes, outputIndex};
		const auto program = compile(graph, weights);
		expect(program.tensors[0].dataBytes == 3 && program.tensors[0].size == 4,
		       "weight padding is incorrect");
		expect(program.tensors[1].dataBytes == 4 && program.tensors[1].weightOffset == 16,
		       "weight alignment is incorrect");
		BaselineDmlProgramRuntime runtime(graph, weights);
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
	Graph graph{1, &spec, 1, nodes, 0};
	const auto valid = spec;
	for (const auto offset : {-2LL, std::numeric_limits<long long>::min()}) {
		spec.byteOffset = offset;
		expectError([&] { compile(graph); }, "invalid input byte offset");
	}
	spec = valid;
	spec.dataType = dtUnknown;
	expectError([&] { compile(graph); }, "Unknown");
	spec = valid;
	spec.shapeRank = 5;
	expectError([&] { compile(graph); }, "shape rank");
	spec = valid;
	spec.shape[0] = 0;
	expectError([&] { compile(graph); }, "positive");
	spec = valid;
	spec.shapeRank = 4;
	spec.shape = {0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu};
	expectError([&] { compile(graph); }, "overflow");
	spec = valid;
	spec.byteOffset = 0;
	expectError([&] { compile(graph); }, "weight exceeds");
	spec = valid;
	nodes[0] = InputItem{dtFloat32, -1};
	expectError([&] { compile(graph); }, "node 0");
	nodes[0] = InputItem{dtFloat32, 1};
	expectError([&] { compile(graph); }, "out of range");
	nodes[0] = InputItem{dtFloat32, 0};
	graph.outputIndex = 1;
	expectError([&] { compile(graph); }, "preceding node");
	const Node invalidEdges[]{InputItem{dtFloat32, 0}, Onnx14Node{Add{dtFloat32, 0, 1, 1, {3}}}};
	graph = {1, &spec, 2, invalidEdges, 1};
	expectError([&] { compile(graph); }, "node 1");
	const InputSpecItem mixedSpecs[]{spec, {kRuntimeInput, dtUInt32, 1, {3}}};
	const Node mismatch[]{InputItem{dtFloat32, 0}, InputItem{dtUInt32, 1},
			      Onnx14Node{Add{dtFloat32, 0, 1, 1, {3}}}};
	graph = {2, mixedSpecs, 3, mismatch, 2};
	expectError([&] { compile(graph); }, "data types must match");
	graph = {1, &spec, 2, nullptr, 1};
	const Node badUnary[]{InputItem{dtFloat32, 0}, Onnx14Node{Relu{dtFloat32, 0, 1, {2}}}};
	graph.nodes = badUnary;
	expectError([&] { compile(graph); }, "element count");
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
	const Graph graph{2, specs, 3, nodes, 2};
	const std::array<std::uint32_t, 3> a{1, 100, 50000}, b{2, 200, 60000};
	std::unique_ptr<BaselineDmlProgramRuntime> runtime;
	try {
		runtime = std::make_unique<BaselineDmlProgramRuntime>(graph, std::as_bytes(std::span(b)));
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
	std::array<std::int32_t, 8> uavBarriers[4];
	for (auto &row : uavBarriers)
		row.fill(-1);
	uavBarriers[3][0] = 2;
	const Graph graph{2, specs, 4, nodes, 3, uavBarriers};
	// IEEE binary16 encodings for [1, 2, 3] + 2 = [3, 4, 5].
	const std::array<std::uint16_t, 3> values{0x3c00, 0x4000, 0x4200}, expected{0x4200, 0x4400, 0x4500};
	const std::array<std::uint16_t, 1> weights{0x4000};
	BaselineDmlProgramRuntime runtime(graph, std::as_bytes(std::span(weights)));
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
	std::array<std::int32_t, 8> uavBarriers[4];
	for (auto &row : uavBarriers)
		row.fill(-1);
	uavBarriers[2][0] = 1;
	uavBarriers[3][0] = 2;
	Graph graph{1, specs, 4, nodes, 3, uavBarriers};
	const auto program = compile(graph);
	expect(program.steps.size() == 3, "unexpected step count");
	expect(program.steps[0].barriersBefore.empty(), "unrequested input barrier");
	for (std::size_t step = 1; step < 3; ++step) {
		const auto &barriers = program.steps[step].barriersBefore;
		expect(barriers.size() == 1, "barrier table was not honored");
		expect(program.tensors[barriers[0]].storage == program.tensors[program.steps[step - 1].output].storage,
		       "barrier targets the wrong producer storage");
	}
	// Compile only: missing synchronization must not be inferred by C++.
	graph.uavBarriers = nullptr;
	expect(compile(graph).steps[1].barriersBefore.empty(), "compiler inferred an unrequested barrier");
	graph.uavBarriers = uavBarriers;
	uavBarriers[2].fill(-1);
	expect(compile(graph).steps[1].barriersBefore.empty(), "unused barrier slots ignored");
	auto &item = std::get<InputItem>(nodes[0]);
	item.dataType = dtFloat16;
	expectError([&] { compile(graph); }, "InputItem and InputSpec");
	for (const auto type : {0, -1, 99}) {
		item.dataType = type;
		expectError([&] { compile(graph); }, "node 0 InputItem");
	}
	item.dataType = dtFloat32;
	std::get<Add>(std::get<Onnx14Node>(nodes[2])).dataType = dtFloat16;
	expectError([&] { compile(graph); }, "node 2 Add (Float16)");
	return 0;
}

int testUavBarrierInstructions()
{
	constexpr InputSpecItem specs[]{{kRuntimeInput, dtFloat32, 1, {4}}};
	const Node nodes[]{InputItem{dtFloat32, 0}, Onnx14Node{Relu{dtFloat32, 0, 1, {4}}},
			   Onnx14Node{Relu{dtFloat32, 0, 1, {4}}}, Onnx14Node{Add{dtFloat32, 1, 2, 1, {4}}}};
	std::array<std::int32_t, 8> uavBarriers[4];
	for (auto &row : uavBarriers)
		row.fill(-1);
	// Synchronize Node 1 before Node 2, although Node 2 does not read it.
	uavBarriers[2][0] = 1;
	uavBarriers[3][0] = 2;
	const Graph graph{1, specs, 4, nodes, 3, uavBarriers};
	const auto program = compile(graph);
	expect(program.steps[1].barriersBefore.size() == 1, "non-input producer instruction ignored");
	const auto first = program.steps[0].output, second = program.steps[1].output;
	expect(program.tensors[program.steps[1].barriersBefore[0]].storage == program.tensors[first].storage,
	       "non-input producer barrier targets wrong resource");
	const std::array<float, 4> input{-2, -1, 1, 2};
	std::array<float, 4> output{};
	const std::array<std::span<const std::byte>, 1> inputs{std::as_bytes(std::span(input))};
	BaselineDmlProgramRuntime runtime(graph, {});
	runtime.run(inputs, std::as_writable_bytes(std::span(output)));
	for (std::size_t i = 0; i < 4; ++i)
		expect(output[i] == 2 * std::max(0.0F, input[i]), "repositioned barrier result differs");
	// Holes are unused slots, not an early terminator. All eight slots are valid.
	uavBarriers[3] = {-1, 1, -1, 2, -1, -1, -1, -1};
	expect(compile(graph).steps[2].barriersBefore.size() == 2, "barrier slots after -1 ignored");
	uavBarriers[3] = {0, 1, 2, 0, 1, 2, 0, 1};
	expect(compile(graph).steps[2].barriersBefore.size() == 8, "eight-slot row or duplicates changed");
	for (const auto invalid : {-2, 3, 4, 999}) {
		uavBarriers[3].fill(-1);
		uavBarriers[3][0] = invalid;
		expectError([&] { compile(graph); }, "UAV barrier must reference a preceding node");
	}
	// A barrier before a non-dispatch InputItem still occupies that boundary.
	const Node inputBoundary[]{InputItem{dtFloat32, 0}, Onnx14Node{Relu{dtFloat32, 0, 1, {4}}},
				   InputItem{dtFloat32, 0}, Onnx14Node{Add{dtFloat32, 1, 2, 1, {4}}}};
	for (auto &row : uavBarriers)
		row.fill(-1);
	uavBarriers[2][0] = 1;
	const Graph boundaryGraph{1, specs, 4, inputBoundary, 3, uavBarriers};
	const auto boundary = compile(boundaryGraph);
	expect(boundary.steps.size() == 3 && boundary.steps[1].output == -1 &&
		       boundary.steps[1].barriersBefore.size() == 1,
	       "InputItem boundary lost");
	BaselineDmlProgramRuntime boundaryRuntime(boundaryGraph, {});
	boundaryRuntime.run(inputs, std::as_writable_bytes(std::span(output)));
	for (std::size_t i = 0; i < 4; ++i)
		expect(output[i] == std::max(0.0F, input[i]) + input[i], "InputItem boundary result differs");
	const Graph trailing{1, specs, 3, inputBoundary, 2, uavBarriers};
	BaselineDmlProgramRuntime trailingRuntime(trailing, {});
	trailingRuntime.run(inputs, std::as_writable_bytes(std::span(output)));
	expect(output == input, "trailing barrier-only step changed pass-through output");
	return 0;
}

int testUavBarrierWeightResource()
{
	const InputSpecItem specs[]{{kRuntimeInput, dtFloat32, 1, {4}}, {0, dtFloat32, 1, {4}}};
	const Node nodes[]{InputItem{dtFloat32, 0}, InputItem{dtFloat32, 1}, Onnx14Node{Add{dtFloat32, 0, 1, 1, {4}}}};
	const std::array<std::int32_t, 8> uavBarriers[]{{-1, -1, -1, -1, -1, -1, -1, -1},
							{-1, -1, -1, -1, -1, -1, -1, -1},
							{1, -1, -1, -1, -1, -1, -1, -1}};
	const std::array<float, 4> weights{1, 2, 3, 4}, input{4, 3, 2, 1};
	std::array<float, 4> output{};
	BaselineDmlProgramRuntime runtime({2, specs, 3, nodes, 2, uavBarriers}, std::as_bytes(std::span(weights)));
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
