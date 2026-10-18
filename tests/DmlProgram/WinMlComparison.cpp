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
#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"
#include "OnnxTestModel.hpp"
#include "MaskReference.hpp"
#include <winrt/Windows.Storage.Streams.h>
#include "LandscapeTestModel.hpp"
#include <DmlProgram/BaselineDmlProgramRuntime.hpp>
#include <winrt/Windows.AI.MachineLearning.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <vector>

void runLandscapeReference(BaselineDml::BaselineDmlProgramRuntime &runtime, std::span<const float> input,
			   std::span<float> output)
{
	if (runtime.inputInfo(0).type != BaselineDml::ProgramDefinition::dtFloat32 ||
	    runtime.outputInfo().type != BaselineDml::ProgramDefinition::dtFloat32) {
		FAIL("Landscape requires FP32 input and output");
	}
	const std::array inputs{std::as_bytes(input)};
	runtime.run(inputs, std::as_writable_bytes(output));
}

#ifdef MEDIAPIPE_LANDSCAPE_ONNX
#include <fstream>
#include <wincodec.h>
#include <wrl/client.h>
#endif

using namespace BaselineDml;
using namespace BaselineDml::ProgramDefinition;
using namespace winrt::Windows::AI::MachineLearning;

TEST_CASE("MPLD10Float32Postprocess")
{
	const InputSpecItem specs[]{
		{kRuntimeInput, dtFloat32, 4, {1, 1, 144, 256}},
		{kRuntimeInput, dtFloat32, 4, {1, 1, 1, 1}},
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
	BaselineDmlProgramRuntime binary(specs, std::span(nodes).subspan(0, 3), {}, 2, {});
	BaselineDmlProgramRuntime full(specs, std::span(nodes).subspan(0, 4), barriers, 3, {});
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
	std::vector<InputSpecItem> specs{{kRuntimeInput, dtFloat32, 4, shape}};
	std::vector<Node> nodes{InputItem{dtFloat32, 0}};
	std::vector<float> weights{3.0F, 1.0F / 6.0F};
	if (name == "PointwiseConvTranspose") {
		weights.resize(68);
		for (size_t phase = 0; phase < 4; ++phase) {
			for (size_t channel = 0; channel < 16; ++channel)
				weights[phase * 16 + channel] = float(int((channel * 4 + phase) % 7) - 3) / 16;
			weights[64 + phase] = 0.25F;
		}
		specs.push_back({0, dtFloat32, 4, {4, 16, 1, 1}});
		specs.push_back({256, dtFloat32, 1, {4}});
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
		specs.push_back({0, dtFloat32, 4, {16, 1, 3, 3}});
		specs.push_back({576, dtFloat32, 1, {16}});
		nodes.insert(nodes.end(), {InputItem{dtFloat32, 1}, InputItem{dtFloat32, 2},
					   FusedNode{MPLD10DepthwiseConv2dRelu{dtFloat32, 0, 1, 2}}});
	} else if (name == "HardSwishFused") {
		nodes.push_back(FusedNode{HardSwishGate{dtFloat32, 0, 4, shape}});
		nodes.push_back(Onnx14Node{Mul{dtFloat32, 0, 1, 4, shape}});
	} else if (name == "HardSwish") {
		specs.push_back({0, dtFloat32, 4, {1, 1, 1, 1}});
		specs.push_back({4, dtFloat32, 4, {1, 1, 1, 1}});
		nodes.insert(nodes.end(),
			     {InputItem{dtFloat32, 1}, InputItem{dtFloat32, 2},
			      Onnx14Node{Add{dtFloat32, 0, 1, 4, shape}}, Onnx13Node{ClipFloat32{3, 4, shape, 0, 6}},
			      Onnx14Node{Mul{dtFloat32, 2, 4, 4, shape}}, Onnx14Node{Mul{dtFloat32, 0, 5, 4, shape}}});
	} else if (name == "ReduceMean") {
		nodes.push_back(Onnx11Node{AveragePool2d{dtFloat32, 0, {1, 1}, {3, 4}, {0, 0}, {0, 0}, outShape}});
	} else if (name == "Resize") {
		nodes.push_back(Onnx13Node{Resize{dtFloat32, 0, 4, 2, 3, outShape}});
	} else if (name == "Add" || name == "Mul") {
		weights = {0.5F, -1.25F};
		specs.push_back({0, dtFloat32, 4, {1, 2, 1, 1}});
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
		weights.resize(count + 2);
		for (size_t i = 0; i < count; ++i)
			weights[i] = float(int(i % 7) - 3) / 16;
		weights[count] = 0.25F;
		weights[count + 1] = -0.5F;
		specs.push_back({0, dtFloat32, 4, filter});
		specs.push_back({static_cast<int64_t>(count * 4), dtFloat32, 1, {2, 0, 0, 0}});
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
	std::span<const InputSpecItem> graphInputSpecs = specs;
	std::span<const Node> graphNodes = std::span(nodes).subspan(0, static_cast<std::int32_t>(nodes.size()));
	std::span<const std::pair<std::int32_t, std::int32_t>> graphBarriers = uavBarriers;
	std::int32_t graphOutputIndex = static_cast<std::int32_t>(nodes.size() - 1);
	BaselineDmlProgramRuntime runtime(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex,
					  std::as_bytes(std::span(weights)));
	for (const auto kind : {LearningModelDeviceKind::Cpu, LearningModelDeviceKind::DirectXHighPerformance}) {
		const char *backend = kind == LearningModelDeviceKind::Cpu ? "CPU" : "GPU";
		SUBCASE(backend)
		{
			try {
				using namespace winrt::Windows::Storage::Streams;
				const auto bytes = OnnxTestModel::build(name, shape, outShape);
				const InMemoryRandomAccessStream stream;
				const DataWriter writer(stream);
				writer.WriteBytes(bytes);
				writer.StoreAsync().get();
				writer.DetachStream();
				stream.Seek(0);
				const auto model = LearningModel::LoadFromStream(
					RandomAccessStreamReference::CreateFromStream(stream));

				const LearningModelSession session(model, LearningModelDevice(kind));
				for (int scenario = 0; scenario < 5; ++scenario) {
					const char *scenarioNames[]{"constant", "signed ramp", "activation boundaries",
								    "first corner", "last corner"};
					SUBCASE(scenarioNames[scenario])
					{
						const auto input = makeInput(scenario, size_t(shape[0]) * shape[1] *
											       shape[2] * shape[3]);
						const LearningModelBinding binding(session);
						binding.Bind(L"x",
							     TensorFloat::CreateFromArray(
								     winrt::single_threaded_vector<int64_t>(
									     {shape[0], shape[1], shape[2], shape[3]}),
								     input));
						const auto result = session.Evaluate(binding, L"comparison");
						const auto reference = result.Outputs()
									       .Lookup(L"y")
									       .as<TensorFloat>()
									       .GetAsVectorView();
						std::vector<float> actual(outShape[0] * outShape[1] * outShape[2] *
									  outShape[3]);
						const std::array inputs{std::as_bytes(std::span(input))};
						runtime.run(inputs, std::as_writable_bytes(std::span(actual)));
						REQUIRE(reference.Size() == actual.size());
						const std::vector<float> expectedValues(reference.begin(),
											reference.end());
						const auto checkExpected = [&](const std::vector<float> &values,
									       const char *implementation) {
							REQUIRE(values.size() == expectedValues.size());
							float maximum = 0;
							size_t worst = 0, bad = 0;
							for (size_t i = 0; i < values.size(); ++i) {
								const float expected = expectedValues[i];
								const float error = std::abs(values[i] - expected);
								if (error > maximum) {
									maximum = error;
									worst = i;
								}
								if (!std::isfinite(values[i]) ||
								    !std::isfinite(expected) ||
								    error > 2e-5F + 2e-5F * std::abs(expected))
									++bad;
							}
							INFO("operator=" << name << ", backend=" << std::string(backend)
									 << ", scenario=" << scenario
									 << ", implementation="
									 << std::string(implementation));
							INFO("max_abs=" << maximum << ", worst_index=" << worst
									<< ", actual=" << values[worst]
									<< ", expected=" << expectedValues[worst]);
							std::cout << name << "," << backend << "," << scenario << ","
								  << implementation << ",max_abs=" << maximum
								  << ",bad=" << bad << "\n";
							CHECK(bad == 0);
						};
						checkExpected(actual, name == "PointwiseConvTranspose"
									      ? "Pointwise projection"
								      : name == "MPLD10DepthwiseConv2dRelu" ? "D3D12 CS"
													    : "DML");
					}
				}

			} catch (const winrt::hresult_error &error) {

				std::cout << name << "," << backend << ",WINML_ERROR,0x" << std::hex
					  << uint32_t(error.code()) << std::dec << ","
					  << winrt::to_string(error.message()) << "\n";
				FAIL("WinML error: " << winrt::to_string(error.message()));
			}
		}
	}
}
} // namespace

int testMain(int argc, char **argv)
{
	winrt::init_apartment();
	doctest::Context context(argc, argv);
	const int result = context.run();
	winrt::uninit_apartment();
	return result;
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

#ifdef MEDIAPIPE_LANDSCAPE_ONNX
namespace {
std::vector<float> portraitInput()
{
	using Microsoft::WRL::ComPtr;
	ComPtr<IWICImagingFactory> factory;
	winrt::check_hresult(
		CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)));
	ComPtr<IWICBitmapDecoder> decoder;
	winrt::check_hresult(factory->CreateDecoderFromFilename(winrt::to_hstring(LANDSCAPE_PORTRAIT).c_str(), nullptr,
								GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder));
	ComPtr<IWICBitmapFrameDecode> frame;
	winrt::check_hresult(decoder->GetFrame(0, &frame));
	ComPtr<IWICBitmapScaler> scaler;
	winrt::check_hresult(factory->CreateBitmapScaler(&scaler));
	winrt::check_hresult(scaler->Initialize(frame.Get(), 256, 144, WICBitmapInterpolationModeLinear));
	ComPtr<IWICFormatConverter> converter;
	winrt::check_hresult(factory->CreateFormatConverter(&converter));
	winrt::check_hresult(converter->Initialize(scaler.Get(), GUID_WICPixelFormat24bppRGB, WICBitmapDitherTypeNone,
						   nullptr, 0, WICBitmapPaletteTypeCustom));
	std::vector<uint8_t> rgb(256 * 144 * 3);
	winrt::check_hresult(converter->CopyPixels(nullptr, 256 * 3, static_cast<UINT>(rgb.size()), rgb.data()));
	std::vector<float> input(rgb.size());
	for (size_t i = 0; i < 256 * 144; ++i)
		for (size_t c = 0; c < 3; ++c)
			input[c * 256 * 144 + i] = float(rgb[3 * i + c]) / 255;
	return input;
}
LearningModel landscapeModel()
{
	using namespace winrt::Windows::Storage::Streams;
	std::ifstream file(MEDIAPIPE_LANDSCAPE_ONNX, std::ios::binary | std::ios::ate);
	if (!file || file.tellg() < 2)
		throw std::runtime_error("Missing original Landscape ONNX");
	std::vector<uint8_t> bytes(static_cast<size_t>(file.tellg()));
	file.seekg(0);
	if (!file.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
		throw std::runtime_error("Cannot read Landscape ONNX");
	// The pinned opset-14 model uses no IR-10-only fields or datatypes.
	// This OS WinML supports IR <= 9. Change only the IR header in memory;
	// all graph nodes, attributes, weights and opsets remain byte-identical.
	if (bytes[0] != 8 || bytes[1] != 10)
		throw std::runtime_error("Unexpected pinned ONNX IR header");
	bytes[1] = 9;
	const InMemoryRandomAccessStream stream;
	const DataWriter writer(stream);
	writer.WriteBytes(bytes);
	writer.StoreAsync().get();
	writer.DetachStream();
	stream.Seek(0);
	return LearningModel::LoadFromStream(RandomAccessStreamReference::CreateFromStream(stream));
}
} // namespace

TEST_CASE("MediaPipeLandscapeMask")
{
	const auto input = portraitInput();
	auto raw = LandscapeTestModel::makeRawRuntime();
	std::vector<float> probabilities(256 * 144);
	REQUIRE(raw->inputCount() == 1);
	runLandscapeReference(*raw, input, probabilities);
	auto runtime = LandscapeTestModel::makeMaskRuntime();
	REQUIRE(runtime->inputCount() == 2);
	REQUIRE(runtime->outputInfo().type == dtFloat32);
	REQUIRE(runtime->outputInfo().dataBytes == 256 * 144 * sizeof(float));
	for (const float threshold : {0.0F, 0.42F, 1.0F}) {
		CAPTURE(threshold);
		const std::array inputs{std::as_bytes(std::span(input)), std::as_bytes(std::span(&threshold, 1))};
		std::vector<float> output(256 * 144);
		runtime->run(inputs, std::as_writable_bytes(std::span(output)));
		const auto expected = BackgroundRemoval::PipelineTest::coarseMask(probabilities, threshold);
		double maxError = 0;
		for (std::size_t i = 0; i < output.size(); ++i)
			maxError = std::max(maxError, std::abs(double(output[i]) - expected[i]));
		CHECK(maxError < 1e-6);
	}
}

TEST_CASE("MediaPipeLandscape")
{
	constexpr size_t pixels = 144 * 256;
	static auto runtime = LandscapeTestModel::makeRawRuntime();
	const auto model = landscapeModel();
	for (const auto kind : {LearningModelDeviceKind::Cpu, LearningModelDeviceKind::DirectXHighPerformance}) {
		const char *backend = kind == LearningModelDeviceKind::Cpu ? "CPU" : "GPU";
		SUBCASE(backend)
		{
			try {
				const LearningModelSession session(model, LearningModelDevice(kind));
				for (int scenario = 0; scenario < 5; ++scenario) {
					const char *names[]{"zeros", "constant gray", "RGB gradient", "seeded pixels",
							    "portrait"};
					SUBCASE(names[scenario])
					{
						std::vector<float> input(3 * pixels);
						uint32_t state = 42;
						for (size_t c = 0; c < 3; ++c)
							for (size_t y = 0; y < 144; ++y)
								for (size_t x = 0; x < 256; ++x) {
									state = state * 1664525u + 1013904223u;
									input[c * pixels + y * 256 + x] =
										scenario == 0   ? 0
										: scenario == 1 ? 0.5F
										: scenario == 2
											? (c == 0 ? float(x) / 255
											   : c == 1
												   ? float(y) / 143
												   : float(x + y) / 398)
											: float(state >> 8) / 16777215;
								}
						if (scenario == 4)
							input = portraitInput();
						const LearningModelBinding binding(session);
						binding.Bind(model.InputFeatures().GetAt(0).Name(),
							     TensorFloat::CreateFromArray(
								     winrt::single_threaded_vector<int64_t>(
									     {1, 3, 144, 256}),
								     input));
						const auto result = session.Evaluate(binding, L"Landscape comparison");
						const auto expected =
							result.Outputs()
								.Lookup(model.OutputFeatures().GetAt(0).Name())
								.as<TensorFloat>()
								.GetAsVectorView();
						REQUIRE(expected.Size() == pixels);
						std::vector<float> actual(pixels);
						const std::array inputs{std::as_bytes(std::span(input))};
						runLandscapeReference(*runtime, input, actual);
						float maximum = 0;
						double sum = 0, squares = 0;
						size_t worst = 0, bad = 0, maskDisagreements = 0;
						for (size_t i = 0; i < pixels; ++i) {
							const float reference =
								expected.GetAt(static_cast<uint32_t>(i));
							const float error = std::abs(actual[i] - reference);
							maskDisagreements += (actual[i] >= 0.5F) != (reference >= 0.5F);
							if (error > maximum) {
								maximum = error;
								worst = i;
							}
							sum += error;
							squares += double(error) * error;
							if (!std::isfinite(actual[i]) || !std::isfinite(reference) ||
							    error > 2e-5F + 2e-5F * std::abs(reference))
								++bad;
						}
						std::cout << "Landscape," << backend << "," << names[scenario]
							  << ",max_abs=" << maximum << ",mean_abs=" << sum / pixels
							  << ",rmse=" << std::sqrt(squares / pixels) << ",bad=" << bad
							  << ",mask_disagreements=" << maskDisagreements
							  << ",worst=" << worst << std::endl;
						INFO("backend=" << std::string(backend)
								<< ", input=" << std::string(names[scenario]));
						INFO("max_abs=" << maximum << ", mean_abs=" << sum / pixels
								<< ", bad=" << bad << ", worst=" << worst
								<< ", DML=" << actual[worst] << ", WinML="
								<< expected.GetAt(static_cast<uint32_t>(worst)));
						CHECK(bad == 0);
					}
				}
			} catch (const winrt::hresult_error &error) {
				FAIL("WinML full-model error: " << winrt::to_string(error.message()));
			}
		}
	}
}

#endif

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
	std::array<InputSpecItem, 3> specs{{
		{kRuntimeInput, dtFloat32, 4, {1, 16, 72, 128}},
		{0, dtFloat32, 4, {16, 1, 3, 3}},
		{576, dtFloat32, 1, {16}},
	}};
	const std::array<float, 160> weights{};
	std::array<Node, 4> nodes{InputItem{dtFloat32, 0}, InputItem{dtFloat32, 1}, InputItem{dtFloat32, 2},
				  FusedNode{MPLD10DepthwiseConv2dRelu{dtFloat32, 0, 1, 2}}};
	std::span<const InputSpecItem> graphInputSpecs = specs;
	std::span<const Node> graphNodes = std::span(nodes).subspan(0, static_cast<std::int32_t>(nodes.size()));
	std::span<const std::pair<std::int32_t, std::int32_t>> graphBarriers = {};
	std::int32_t graphOutputIndex = 3;
	SUBCASE("input shape")
	{
		specs[0].shape[3] = 127;
	}
	SUBCASE("filter shape")
	{
		specs[1].shape[2] = 2;
	}
	SUBCASE("bias shape")
	{
		specs[2].shape[0] = 15;
	}
	SUBCASE("runtime filter")
	{
		specs[1].byteOffset = kRuntimeInput;
	}
	SUBCASE("input type")
	{
		specs[0].dataType = dtFloat16;
		nodes[0] = InputItem{dtFloat16, 0};
	}
	CHECK_THROWS_AS(BaselineDmlProgramRuntime(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex,
						  std::as_bytes(std::span(weights))),
			std::runtime_error);
}

TEST_CASE("ClipFloat32Contract")
{
	InputSpecItem spec{kRuntimeInput, dtFloat32, 1, {3}};
	std::array<Node, 2> nodes{InputItem{dtFloat32, 0}, Onnx13Node{ClipFloat32{0, 1, {3}, 0, 6}}};
	std::span<const InputSpecItem> graphInputSpecs = std::span(&spec, 1);
	std::span<const Node> graphNodes = std::span(nodes).subspan(0, 2);
	std::span<const std::pair<std::int32_t, std::int32_t>> graphBarriers = {};
	std::int32_t graphOutputIndex = 1;
	SUBCASE("Float16 input")
	{
		spec.dataType = dtFloat16;
		nodes[0] = InputItem{dtFloat16, 0};
	}
	SUBCASE("integer input")
	{
		spec.dataType = dtInt32;
		nodes[0] = InputItem{dtInt32, 0};
	}
	SUBCASE("reversed bounds")
	{
		nodes[1] = Onnx13Node{ClipFloat32{0, 1, {3}, 6, 0}};
	}
	CHECK_THROWS_AS(BaselineDmlProgramRuntime(graphInputSpecs, graphNodes, graphBarriers, graphOutputIndex, {}),
			std::runtime_error);
}
