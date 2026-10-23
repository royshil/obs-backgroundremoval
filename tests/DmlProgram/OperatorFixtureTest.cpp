// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <TraceLoggingProvider.h>
#include <winrt/base.h>
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>
#include "../DoctestRunner.hpp"
#include "MaskReference.hpp"
#include "ProgramTestSession.hpp"
#include "TensorSpecFixture.hpp"
#include <BaselineDml/DmlProgramCompiler.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <string>
#include <vector>

TRACELOGGING_DECLARE_PROVIDER(g_hProvider);
using namespace BaselineDml;
using DmlProgramTest::ProgramTestSession;
using DmlProgramTest::makeInputTensorSpec;
using DmlProgramTest::makeWeightTensorSpec;
using namespace BaselineDml::Definition;

TEST_CASE("MPLD10Float32Postprocess")
{
	const TensorSpec specs[]{
		makeInputTensorSpec(dtFloat32, 4, {1, 1, 144, 256}),
		makeInputTensorSpec(dtFloat32, 4, {1, 1, 1, 1}),
	};
	const std::pair<std::int32_t, std::int32_t> barriers[]{{3, 2}};
	std::vector<float> probabilities(256 * 144);
	const std::array<float, 8> boundary{0.0F,  1.0F,  0.5F, std::nextafter(0.5F, 0.0F), std::nextafter(0.5F, 1.0F),
					    0.25F, 0.75F, 0.42F};
	for (std::size_t i = 0; i < probabilities.size(); ++i)
		probabilities[i] = boundary[(i + i / 256) % boundary.size()];
	const Node nodes[]{
		InputItem{dtFloat32, 0},
		InputItem{dtFloat32, 1},
		FusedNode{MPLD10BinarizeFloat32{0, 1}},
		Onnx11Node{AveragePool2d{dtFloat32, 2, {1, 1}, {5, 5}, {2, 2}, {2, 2}, {1, 1, 144, 256}}},
	};
	ProgramTestSession binary(specs, std::span(nodes).subspan(0, 3), {}, 2, {});
	ProgramTestSession full(specs, std::span(nodes).subspan(0, 4), barriers, 3, {});
	for (const float threshold : {0.0F, 0.42F, 0.5F, 1.0F}) {
		CAPTURE(threshold);
		const std::array inputs{std::as_bytes(std::span(probabilities)),
					std::as_bytes(std::span(&threshold, 1))};
		std::vector<float> output(probabilities.size());
		binary.run(inputs, std::as_writable_bytes(std::span(output)));
		for (std::size_t i = 0; i < output.size(); ++i)
			REQUIRE(output[i] == (probabilities[i] < threshold ? 1.0F : 0.0F));
		const auto expected = BackgroundRemoval::PipelineTest::coarseMask(probabilities, threshold);
		full.run(inputs, std::as_writable_bytes(std::span(output)));
		double maxError = 0;
		for (std::size_t i = 0; i < output.size(); ++i)
			maxError = std::max(maxError, std::abs(double(output[i]) - expected[i]));
		CHECK(maxError < 1e-6);
	}
}

namespace {
std::vector<float> makeInput(int scenario, size_t count = 24)
{
	const std::array<float, 12> boundary{-100, -3.00001F, -3, -2.99999F, -1, 0, 1, 2.99999F, 3, 3.00001F, 10, 100};
	std::vector<float> values(count, 0);
	for (size_t i = 0; i < values.size(); ++i)
		values[i] = scenario == 0   ? 0.25F
			    : scenario == 1 ? float(i % 24) - 12
			    : scenario == 2 ? boundary[i % 12]
					    : 0;
	if (scenario == 3)
		values.front() = 1;
	if (scenario == 4)
		values.back() = 1;
	return values;
}

void compareOperator(const std::string &name)
{
	const std::array<std::int32_t, 4> shape =
		(name == "PointwiseConvTranspose" || name == "MPLD10DepthwiseConv2dRelu")
			? std::array<std::int32_t, 4>{1, 16, 72, 128}
			: std::array<std::int32_t, 4>{1, 2, 3, 4};
	const std::array<std::int32_t, 4> outShape = name == "PointwiseConvTranspose"
							     ? std::array<std::int32_t, 4>{1, 1, 144, 256}
						     : name == "ReduceMean" ? std::array<std::int32_t, 4>{1, 2, 1, 1}
						     : name == "Resize"     ? std::array<std::int32_t, 4>{1, 2, 6, 12}
						     : name == "ConvTranspose" ? std::array<std::int32_t, 4>{1, 2, 6, 8}
									       : shape;
	std::vector<TensorSpec> specs{makeInputTensorSpec(dtFloat32, 4, shape)};
	std::vector<Node> nodes{InputItem{dtFloat32, 0}};
	std::vector<float> weights{3.0F, 1.0F / 6.0F};
	if (name == "PointwiseConvTranspose") {
		weights.resize(68);
		for (size_t phase = 0; phase < 4; ++phase) {
			for (size_t channel = 0; channel < 16; ++channel)
				weights[phase * 16 + channel] = float(int((channel * 4 + phase) % 7) - 3) / 16;
			weights[64 + phase] = 0.25F;
		}
		specs.push_back(makeWeightTensorSpec(0, dtFloat32, 4, {4, 16, 1, 1}));
		specs.push_back(makeWeightTensorSpec(256, dtFloat32, 1, {4}));
		nodes.insert(
			nodes.end(),
			{InputItem{dtFloat32, 1}, InputItem{dtFloat32, 2},
			 Onnx11Node{
				 BiasedConv2d{dtFloat32, 0, 1, 2, {1, 1}, {1, 1}, {0, 0}, {0, 0}, 1, {1, 4, 72, 128}}},
			 Onnx11Node{DepthToSpace{dtFloat32, 3, 2, outShape}}});
	} else if (name == "MPLD10DepthwiseConv2dRelu") {
		weights.resize(160);
		for (size_t i = 0; i < 144; ++i)
			weights[i] = float(int(i % 7) - 3) / 16;
		for (size_t i = 0; i < 16; ++i)
			weights[144 + i] = i % 2 ? -0.5F : 0.25F;
		specs.push_back(makeWeightTensorSpec(0, dtFloat32, 4, {16, 1, 3, 3}));
		specs.push_back(makeWeightTensorSpec(576, dtFloat32, 1, {16}));
		nodes.insert(nodes.end(), {InputItem{dtFloat32, 1}, InputItem{dtFloat32, 2},
					   FusedNode{MPLD10DepthwiseConv2dRelu{dtFloat32, 0, 1, 2}}});
	} else if (name == "HardSwishFused") {
		nodes.push_back(FusedNode{HardSwishGate{dtFloat32, 0, 4, shape}});
		nodes.push_back(Onnx14Node{Mul{dtFloat32, 0, 1, 4, shape}});
	} else if (name == "HardSwish") {
		const size_t count = size_t(shape[0]) * shape[1] * shape[2] * shape[3];
		weights.assign(count, 3.0F);
		weights.resize(count * 2, 1.0F / 6.0F);
		specs.push_back(makeWeightTensorSpec(0, dtFloat32, 4, shape));
		specs.push_back(makeWeightTensorSpec(static_cast<int64_t>(count * 4), dtFloat32, 4, shape));
		nodes.insert(nodes.end(),
			     {InputItem{dtFloat32, 1}, InputItem{dtFloat32, 2},
			      Onnx14Node{Add{dtFloat32, 0, 1, 4, shape}}, Onnx13Node{ClipFloat32{3, 4, shape, 0, 6}},
			      Onnx14Node{Mul{dtFloat32, 2, 4, 4, shape}}, Onnx14Node{Mul{dtFloat32, 0, 5, 4, shape}}});
	} else if (name == "ReduceMean") {
		nodes.push_back(Onnx11Node{AveragePool2d{dtFloat32, 0, {1, 1}, {3, 4}, {0, 0}, {0, 0}, outShape}});
	} else if (name == "Resize") {
		nodes.push_back(Onnx13Node{Resize{dtFloat32, 0, 4, 2, 3, outShape}});
	} else if (name == "Add" || name == "Mul") {
		const size_t channelSize = size_t(shape[2]) * shape[3];
		weights.assign(channelSize, 0.5F);
		weights.resize(channelSize * 2, -1.25F);
		specs.push_back(makeWeightTensorSpec(0, dtFloat32, 4, shape));
		nodes.push_back(InputItem{dtFloat32, 1});
		if (name == "Add")
			nodes.push_back(Onnx14Node{Add{dtFloat32, 0, 1, 4, shape}});
		else
			nodes.push_back(Onnx14Node{Mul{dtFloat32, 0, 1, 4, shape}});
	} else if (name == "Relu") {
		nodes.push_back(Onnx14Node{Relu{dtFloat32, 0, 4, shape}});
	} else if (name == "Sigmoid") {
		nodes.push_back(Onnx13Node{Sigmoid{dtFloat32, 0, 4, shape}});
	} else if (name == "Clip") {
		nodes.push_back(Onnx13Node{ClipFloat32{0, 4, shape, 0, 6}});
	} else {
		const bool depthwise = name == "DepthwiseConv" || name == "DepthwiseConvRelu",
			   transpose = name == "ConvTranspose";
		const std::int32_t kernel = transpose ? 2 : 3;
		const std::array<std::int32_t, 4> filter{2, depthwise ? 1 : 2, kernel, kernel};
		const size_t count = filter[0] * filter[1] * kernel * kernel;
		const size_t biasOffset = (count + 3) / 4 * 4;
		weights.resize(biasOffset + 2);
		for (size_t i = 0; i < count; ++i)
			weights[i] = float(int(i % 7) - 3) / 16;
		weights[biasOffset] = 0.25F;
		weights[biasOffset + 1] = -0.5F;
		specs.push_back(makeWeightTensorSpec(0, dtFloat32, 4, filter));
		specs.push_back(makeWeightTensorSpec(static_cast<int64_t>(biasOffset * 4), dtFloat32, 1, {2, 0, 0, 0}));
		nodes.push_back(InputItem{dtFloat32, 1});
		nodes.push_back(InputItem{dtFloat32, 2});
		if (transpose)
			nodes.push_back(Onnx11Node{
				BiasedConvTranspose2d{dtFloat32, 0, 1, 2, {2, 2}, {1, 1}, {0, 0}, {0, 0}, 1, outShape}});
		else if (name == "ConvRelu" || name == "DepthwiseConvRelu")
			nodes.push_back(FusedNode{BiasedConv2dRelu{dtFloat32,
								   0,
								   1,
								   2,
								   {1, 1},
								   {1, 1},
								   {1, 1},
								   {1, 1},
								   depthwise ? 2u : 1u,
								   outShape}});
		else
			nodes.push_back(Onnx11Node{BiasedConv2d{dtFloat32,
								0,
								1,
								2,
								{1, 1},
								{1, 1},
								{1, 1},
								{1, 1},
								depthwise ? 2u : 1u,
								outShape}});
	}
	std::vector<std::pair<std::int32_t, std::int32_t>> uavBarriers;
	if (name == "HardSwish") {
		uavBarriers = {{4, 3}, {5, 4}, {6, 5}};
	} else if (name == "HardSwishFused")
		uavBarriers.emplace_back(2, 1);
	if (name == "PointwiseConvTranspose")
		uavBarriers.emplace_back(4, 3);
	std::span<const TensorSpec> graphTensorSpecs = specs;
	std::span<const Node> graphNodes = std::span(nodes).subspan(0, static_cast<std::int32_t>(nodes.size()));
	std::span<const std::pair<std::int32_t, std::int32_t>> graphBarriers = uavBarriers;
	std::int32_t graphOutputIndex = static_cast<std::int32_t>(nodes.size() - 1);
	ProgramTestSession runtime(graphTensorSpecs, graphNodes, graphBarriers, graphOutputIndex,
				   std::as_bytes(std::span(weights)));
	for (int scenario = 0; scenario < 5; ++scenario) {
		CAPTURE(name);
		CAPTURE(scenario);
		const auto input = makeInput(scenario, size_t(shape[0]) * shape[1] * shape[2] * shape[3]);
		std::vector<float> expected(outShape[0] * outShape[1] * outShape[2] * outShape[3]);
		std::ifstream fixture(std::string(OPERATOR_FIXTURES) + "/" + name + ".f32",
				      std::ios::binary | std::ios::ate);
		REQUIRE(fixture.is_open());
		REQUIRE(fixture.tellg() == static_cast<std::streamoff>(5 * expected.size() * sizeof(float)));
		fixture.seekg(static_cast<std::streamoff>(scenario * expected.size() * sizeof(float)));
		REQUIRE(bool(fixture.read(reinterpret_cast<char *>(expected.data()),
					  static_cast<std::streamsize>(expected.size() * sizeof(float)))));
		std::vector<float> actual(expected.size());
		const std::array inputs{std::as_bytes(std::span(input))};
		runtime.run(inputs, std::as_writable_bytes(std::span(actual)));
		std::size_t bad = 0;
		for (std::size_t i = 0; i < actual.size(); ++i)
			bad += !std::isfinite(actual[i]) || !std::isfinite(expected[i]) ||
			       std::abs(actual[i] - expected[i]) > 2e-5F + 2e-5F * std::abs(expected[i]);
		CAPTURE(bad);
		CHECK(bad == 0);
	}
}
} // namespace

int testMain(int argc, char **argv)
{
	winrt::init_apartment();
	doctest::Context context(argc, argv);
	const int result = TestHelpers::runTests(context);
	winrt::uninit_apartment();
	return result;
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

TEST_CASE("HardSwishFused")
{
	compareOperator("HardSwishFused");
}

TEST_CASE("HardSwish")
{
	compareOperator("HardSwish");
}

TEST_CASE("ReduceMean")
{
	compareOperator("ReduceMean");
}

TEST_CASE("Resize")
{
	compareOperator("Resize");
}

TEST_CASE("Add")
{
	compareOperator("Add");
}

TEST_CASE("Mul")
{
	compareOperator("Mul");
}

TEST_CASE("Relu")
{
	compareOperator("Relu");
}

TEST_CASE("Sigmoid")
{
	compareOperator("Sigmoid");
}

TEST_CASE("ClipFloat32")
{
	compareOperator("Clip");
}

TEST_CASE("Conv")
{
	compareOperator("Conv");
}

TEST_CASE("DepthwiseConv")
{
	compareOperator("DepthwiseConv");
}

TEST_CASE("ConvTranspose")
{
	compareOperator("ConvTranspose");
}

TEST_CASE("ConvRelu")
{
	compareOperator("ConvRelu");
}
TEST_CASE("DepthwiseConvRelu")
{
	compareOperator("DepthwiseConvRelu");
}

TEST_CASE("PointwiseConvTranspose")
{
	compareOperator("PointwiseConvTranspose");
}

TEST_CASE("MPLD10DepthwiseConv2dRelu")
{
	compareOperator("MPLD10DepthwiseConv2dRelu");
}

TEST_CASE("MPLD10DepthwiseConv2dReluContract")
{
	std::array<TensorSpec, 3> specs{{
		makeInputTensorSpec(dtFloat32, 4, {1, 16, 72, 128}),
		makeWeightTensorSpec(0, dtFloat32, 4, {16, 1, 3, 3}),
		makeWeightTensorSpec(576, dtFloat32, 1, {16}),
	}};
	const std::array<float, 160> weights{};
	std::array<Node, 4> nodes{InputItem{dtFloat32, 0}, InputItem{dtFloat32, 1}, InputItem{dtFloat32, 2},
				  FusedNode{MPLD10DepthwiseConv2dRelu{dtFloat32, 0, 1, 2}}};
	std::span<const TensorSpec> graphTensorSpecs = specs;
	std::span<const Node> graphNodes = std::span(nodes).subspan(0, static_cast<std::int32_t>(nodes.size()));
	std::span<const std::pair<std::int32_t, std::int32_t>> graphBarriers = {};
	std::int32_t graphOutputIndex = 3;
	const char *expected = "";
	SUBCASE("input shape")
	{
		specs[0] = makeInputTensorSpec(dtFloat32, 4, {1, 16, 72, 127});
		expected = "MPLD10DepthwiseConv2dRelu input shape";
	}
	SUBCASE("filter shape")
	{
		specs[1] = makeWeightTensorSpec(0, dtFloat32, 4, {16, 1, 2, 3});
		expected = "MPLD10DepthwiseConv2dRelu filter shape";
	}
	SUBCASE("bias shape")
	{
		specs[2] = makeWeightTensorSpec(576, dtFloat32, 1, {15});
		expected = "MPLD10DepthwiseConv2dRelu bias shape";
	}
	SUBCASE("runtime filter")
	{
		specs[1] = makeInputTensorSpec(dtFloat32, 4, {16, 1, 3, 3});
		expected = "MPLD10DepthwiseConv2dRelu filter shape";
	}
	SUBCASE("input type")
	{
		specs[0] = makeInputTensorSpec(dtFloat16, 4, {1, 16, 72, 128});
		nodes[0] = InputItem{dtFloat16, 0};
		expected = "input and operator data types must match; no implicit conversion";
	}
	CHECK_THROWS_WITH_AS(ProgramTestSession(graphTensorSpecs, graphNodes, graphBarriers, graphOutputIndex,
						std::as_bytes(std::span(weights))),
			     expected, DmlCompilationError);
}

TEST_CASE("ClipFloat32Contract")
{
	auto spec = makeInputTensorSpec(dtFloat32, 1, {3});
	std::array<Node, 2> nodes{InputItem{dtFloat32, 0}, Onnx13Node{ClipFloat32{0, 1, {3}, 0, 6}}};
	std::span<const TensorSpec> graphTensorSpecs = std::span(&spec, 1);
	std::span<const Node> graphNodes = std::span(nodes).subspan(0, 2);
	std::span<const std::pair<std::int32_t, std::int32_t>> graphBarriers = {};
	std::int32_t graphOutputIndex = 1;
	const char *expected = "input and operator data types must match; no implicit conversion";
	SUBCASE("Float16 input")
	{
		spec = makeInputTensorSpec(dtFloat16, 1, {3});
		nodes[0] = InputItem{dtFloat16, 0};
	}
	SUBCASE("integer input")
	{
		spec = makeInputTensorSpec(dtInt32, 1, {3});
		nodes[0] = InputItem{dtInt32, 0};
	}
	SUBCASE("reversed bounds")
	{
		nodes[1] = Onnx13Node{ClipFloat32{0, 1, {3}, 6, 0}};
		expected = "ClipFloat32 minimum exceeds maximum";
	}
	CHECK_THROWS_WITH_AS(ProgramTestSession(graphTensorSpecs, graphNodes, graphBarriers, graphOutputIndex, {}),
			     expected, DmlCompilationError);
}
