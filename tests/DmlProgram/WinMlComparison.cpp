// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
// SPDX-License-Identifier: GPL-3.0-or-later
#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"
#include "OnnxTestModel.hpp"
#include <winrt/Windows.Storage.Streams.h>
#include "DmlProgram/BaselineDmlProgramRuntime.hpp"
#include <winrt/Windows.AI.MachineLearning.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <vector>
#ifdef MEDIAPIPE_LANDSCAPE_ONNX
#include <fstream>
#include <wincodec.h>
#include <wrl/client.h>
#endif

using namespace BaselineDml;
using namespace BaselineDml::ProgramDefinition;
using namespace winrt::Windows::AI::MachineLearning;

namespace {
std::vector<float> makeInput(int scenario)
{
	const std::array<float, 12> boundary{-100, -3.00001F, -3, -2.99999F, -1, 0, 1, 2.99999F, 3, 3.00001F, 10, 100};
	std::vector<float> values(24, 0);
	for (size_t i = 0; i < values.size(); ++i)
		values[i] = scenario == 0   ? 0.25F
			    : scenario == 1 ? float(i) - 12
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
	const std::array<UINT, 4> shape{1, 2, 3, 4};
	const std::array<UINT, 4> outShape = name == "ReduceMean"      ? std::array<UINT, 4>{1, 2, 1, 1}
					     : name == "Resize"        ? std::array<UINT, 4>{1, 2, 6, 12}
					     : name == "ConvTranspose" ? std::array<UINT, 4>{1, 2, 6, 8}
								       : shape;
	std::vector<InputSpecItem> specs{{DataType::Float32, ItemType::RuntimeInput, 4, shape, 0}};
	std::vector<Node> nodes{InputItem{0}};
	std::vector<float> weights{3.0F, 1.0F / 6.0F};
	if (name == "HardSwish") {
		specs.push_back({DataType::Float32, ItemType::Weight, 4, {1, 1, 1, 1}, 0});
		specs.push_back({DataType::Float32, ItemType::Weight, 4, {1, 1, 1, 1}, 4});
		nodes.insert(nodes.end(),
			     {InputItem{1}, InputItem{2}, Onnx14Node{Add{DataType::Float32, 0, 1, 4, shape}},
			      Onnx13Node{Clip{DataType::Float32, 3, 4, shape, 0, 6}},
			      Onnx14Node{Mul{DataType::Float32, 2, 4, 4, shape}},
			      Onnx14Node{Mul{DataType::Float32, 0, 5, 4, shape}}});
	} else if (name == "ReduceMean") {
		nodes.push_back(
			Onnx11Node{AveragePool2d{DataType::Float32, 0, {1, 1}, {3, 4}, {0, 0}, {0, 0}, outShape}});
	} else if (name == "Resize") {
		nodes.push_back(Onnx13Node{Resize{DataType::Float32, 0, 4, 2, 3, outShape}});
	} else if (name == "Add" || name == "Mul") {
		weights = {0.5F, -1.25F};
		specs.push_back({DataType::Float32, ItemType::Weight, 4, {1, 2, 1, 1}, 0});
		nodes.push_back(InputItem{1});
		if (name == "Add")
			nodes.push_back(Onnx14Node{Add{DataType::Float32, 0, 1, 4, shape}});
		else
			nodes.push_back(Onnx14Node{Mul{DataType::Float32, 0, 1, 4, shape}});
	} else if (name == "Relu") {
		nodes.push_back(Onnx14Node{Relu{DataType::Float32, 0, 4, shape}});
	} else if (name == "Sigmoid") {
		nodes.push_back(Onnx13Node{Sigmoid{DataType::Float32, 0, 4, shape}});
	} else if (name == "Clip") {
		nodes.push_back(Onnx13Node{Clip{DataType::Float32, 0, 4, shape, 0, 6}});
	} else {
		const bool depthwise = name == "DepthwiseConv", transpose = name == "ConvTranspose";
		const UINT kernel = transpose ? 2 : 3;
		const std::array<UINT, 4> filter{2, depthwise ? 1u : 2u, kernel, kernel};
		const size_t count = filter[0] * filter[1] * kernel * kernel;
		weights.resize(count + 2);
		for (size_t i = 0; i < count; ++i)
			weights[i] = float(int(i % 7) - 3) / 16;
		weights[count] = 0.25F;
		weights[count + 1] = -0.5F;
		specs.push_back({DataType::Float32, ItemType::Weight, 4, filter, 0});
		specs.push_back(
			{DataType::Float32, ItemType::Weight, 1, {2, 0, 0, 0}, static_cast<int64_t>(count * 4)});
		nodes.push_back(InputItem{1});
		nodes.push_back(InputItem{2});
		if (transpose)
			nodes.push_back(Onnx11Node{BiasedConvTranspose2d{DataType::Float32,
									 0,
									 1,
									 2,
									 {2, 2},
									 {1, 1},
									 {0, 0},
									 {0, 0},
									 1,
									 outShape}});
		else
			nodes.push_back(Onnx11Node{BiasedConv2d{DataType::Float32,
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
	const Graph graph{specs.size(), specs.data(), static_cast<int64_t>(nodes.size()), nodes.data(),
			  nodes.size() - 1};
	BaselineDmlProgramRuntime runtime(graph, std::as_bytes(std::span(weights)));
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
						const auto input = makeInput(scenario);
						const LearningModelBinding binding(session);
						binding.Bind(L"x", TensorFloat::CreateFromArray(
									   winrt::single_threaded_vector<int64_t>(
										   {1, 2, 3, 4}),
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
						checkExpected(actual, "DML");
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

int main(int argc, char **argv)
{
	winrt::init_apartment();
	doctest::Context context(argc, argv);
	const int result = context.run();
	winrt::uninit_apartment();
	return result;
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

TEST_CASE("Clip")
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
namespace BaselineDml {
auto makeMediaPipeLandscapeDmlProgram() -> std::unique_ptr<BaselineDmlProgramRuntime>;
}

TEST_CASE("MediaPipeLandscape")
{
	constexpr size_t pixels = 144 * 256;
	static auto runtime = makeMediaPipeLandscapeDmlProgram();
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
						runtime->run(inputs, std::as_writable_bytes(std::span(actual)));
						float maximum = 0;
						double sum = 0, squares = 0;
						size_t worst = 0, bad = 0;
						for (size_t i = 0; i < pixels; ++i) {
							const float reference =
								expected.GetAt(static_cast<uint32_t>(i));
							const float error = std::abs(actual[i] - reference);
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

// Opt-in timing experiment; intentionally not registered with CTest.
TEST_CASE("MediaPipeLandscapeTiming")
{
	using Clock = std::chrono::steady_clock;
	const auto started = Clock::now();
	auto runtime = makeMediaPipeLandscapeDmlProgram();
	const double initialization = std::chrono::duration<double, std::milli>(Clock::now() - started).count();
	const auto input = portraitInput();
	std::vector<float> output(144 * 256);
	const std::array inputs{std::as_bytes(std::span(input))};
	for (int i = 0; i < 10; ++i)
		runtime->run(inputs, std::as_writable_bytes(std::span(output)));
	std::vector<double> samples;
	double sum = 0;
	for (int i = 0; i < 200; ++i) {
		const auto before = Clock::now();
		runtime->run(inputs, std::as_writable_bytes(std::span(output)));
		const double milliseconds = std::chrono::duration<double, std::milli>(Clock::now() - before).count();
		samples.push_back(milliseconds);
		sum += milliseconds;
	}
	std::sort(samples.begin(), samples.end());
	REQUIRE(std::all_of(output.begin(), output.end(), [](float value) { return std::isfinite(value); }));
	std::cout << "DML_TIMING initialization_ms=" << initialization << ",samples=" << samples.size()
		  << ",mean_ms=" << sum / samples.size() << ",median_ms=" << (samples[99] + samples[100]) / 2
		  << ",p95_ms=" << samples[189] << ",min_ms=" << samples.front() << ",max_ms=" << samples.back()
		  << std::endl;
}
#endif
