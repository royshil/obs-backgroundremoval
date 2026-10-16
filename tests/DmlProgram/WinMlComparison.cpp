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
#include <DirectXPackedVector.h>

void runLandscapeReference(BaselineDml::BaselineDmlProgramRuntime &runtime, std::span<const float> input, std::span<float> output)
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
	const std::array<UINT, 4> shape = (name == "PointwiseConvTranspose" || name == "MPLDDepthwiseConv2dRelu") ? std::array<UINT, 4>{1, 16, 72, 128} : std::array<UINT, 4>{1, 2, 3, 4};
	const std::array<UINT, 4> outShape = name == "PointwiseConvTranspose" ? std::array<UINT, 4>{1, 1, 144, 256}
					     : name == "ReduceMean"      ? std::array<UINT, 4>{1, 2, 1, 1}
					     : name == "Resize"          ? std::array<UINT, 4>{1, 2, 6, 12}
					     : name == "ConvTranspose"   ? std::array<UINT, 4>{1, 2, 6, 8}
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
		nodes.insert(nodes.end(), {InputItem{dtFloat32, 1}, InputItem{dtFloat32, 2},
			Onnx11Node{BiasedConv2d{dtFloat32, 0, 1, 2, {1, 1}, {1, 1},
				{0, 0}, {0, 0}, 1, {1, 4, 72, 128}}},
			Onnx11Node{DepthToSpace{dtFloat32, 3, 2, outShape}}});
	} else if (name == "MPLDDepthwiseConv2dRelu") {
		weights.resize(160);
		for (size_t i = 0; i < 144; ++i)
			weights[i] = float(int(i % 7) - 3) / 16;
		for (size_t i = 0; i < 16; ++i)
			weights[144 + i] = i % 2 ? -0.5F : 0.25F;
		specs.push_back({0, dtFloat32, 4, {16, 1, 3, 3}});
		specs.push_back({576, dtFloat32, 1, {16}});
		nodes.insert(nodes.end(), {InputItem{dtFloat32, 1}, InputItem{dtFloat32, 2},
			FusedNode{MPLDDepthwiseConv2dRelu{dtFloat32, 0, 1, 2}}});
	} else if (name == "HardSwishFused") {
		nodes.push_back(FusedNode{HardSwishGate{dtFloat32, 0, 4, shape}});
		nodes.push_back(Onnx14Node{Mul{dtFloat32, 0, 1, 4, shape}});
	} else if (name == "HardSwish") {
		specs.push_back({0, dtFloat32, 4, {1, 1, 1, 1}});
		specs.push_back({4, dtFloat32, 4, {1, 1, 1, 1}});
		nodes.insert(nodes.end(),
			     {InputItem{dtFloat32, 1}, InputItem{dtFloat32, 2},
			      Onnx14Node{Add{dtFloat32, 0, 1, 4, shape}},
			      Onnx13Node{ClipFloat32{3, 4, shape, 0, 6}},
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
		const UINT kernel = transpose ? 2 : 3;
		const std::array<UINT, 4> filter{2, depthwise ? 1u : 2u, kernel, kernel};
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
	std::vector<std::array<std::int32_t, 8>> uavBarriers(nodes.size());
	for (auto &row : uavBarriers)
		row.fill(-1);
	if (name == "HardSwish") {
		uavBarriers[4][0] = 3;
		uavBarriers[5][0] = 4;
		uavBarriers[6][0] = 5;
	} else if (name == "HardSwishFused")
		uavBarriers[2][0] = 1;
	if (name == "PointwiseConvTranspose")
		uavBarriers[4][0] = 3;
	const Graph graph{specs.size(), specs.data(),     static_cast<int64_t>(nodes.size()),
			  nodes.data(), nodes.size() - 1, uavBarriers.data()};
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
						const auto input = makeInput(scenario, size_t(shape[0]) * shape[1] * shape[2] * shape[3]);
						const LearningModelBinding binding(session);
						binding.Bind(L"x", TensorFloat::CreateFromArray(
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
						checkExpected(actual, name == "PointwiseConvTranspose" ? "Pointwise projection" : name == "MPLDDepthwiseConv2dRelu" ? "D3D12 CS" : "DML");
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
	if (argc == 1 && GetModuleHandleW(L"WinPixGpuCapturer.dll")) {
		context.setOption("test-case", "MediaPipeLandscapePixWorkload");
		context.setOption("no-skip", true);
	}
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
namespace BackgroundRemoval::Generated {
auto makeMediaPipeLandscapeDmlProgram() -> std::unique_ptr<BaselineDml::BaselineDmlProgramRuntime>;
#ifdef DML_MODEL_DIAGNOSTICS
auto mediaPipeLandscapeDiagnosticGraph() -> BaselineDml::Graph;
auto mediaPipeLandscapeDiagnosticWeights() -> std::span<const std::byte>;
#endif
}

TEST_CASE("MediaPipeLandscape")
{
	constexpr size_t pixels = 144 * 256;
	static auto runtime = BackgroundRemoval::Generated::makeMediaPipeLandscapeDmlProgram();
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
                        // Opt-in artifacts for precision diagnosis; comparison criteria stay unchanged.
                        if (std::string_view(names[scenario]) == "portrait") {
                            std::array<wchar_t, 4096> directory{};
                            const DWORD length = GetEnvironmentVariableW(L"DML_COMPARISON_OUTPUT_DIR", directory.data(), static_cast<DWORD>(directory.size()));
                            if (length && length < directory.size()) {
                                const std::filesystem::path destination(directory.data());
                                std::filesystem::create_directories(destination);
                                const auto write = [&](std::span<const float> values, const char *suffix) {
                                    std::ofstream file(destination / (std::string(backend) + suffix), std::ios::binary);
                                    file.write(reinterpret_cast<const char *>(values.data()), static_cast<std::streamsize>(values.size_bytes()));
                                    REQUIRE(file.good());
                                };
                                const std::vector<float> reference(expected.begin(), expected.end());
                                write(reference, "-reference.f32");
                                write(actual, "-actual.f32");
                                write(input, "-input.f32");
                                std::vector<DirectX::PackedVector::HALF> halfInput(input.size());
                                std::vector<float> roundedInput(input.size());
                                DirectX::PackedVector::XMConvertFloatToHalfStream(halfInput.data(), sizeof(std::uint16_t), input.data(), sizeof(float), input.size());
                                DirectX::PackedVector::XMConvertHalfToFloatStream(roundedInput.data(), sizeof(float), halfInput.data(), sizeof(std::uint16_t), input.size());
                                const LearningModelBinding roundedBinding(session);
                                roundedBinding.Bind(model.InputFeatures().GetAt(0).Name(), TensorFloat::CreateFromArray(winrt::single_threaded_vector<int64_t>({1, 3, 144, 256}), roundedInput));
                                const auto roundedResult = session.Evaluate(roundedBinding, L"FP16 input isolation");
                                const auto roundedView = roundedResult.Outputs().Lookup(model.OutputFeatures().GetAt(0).Name()).as<TensorFloat>().GetAsVectorView();
                                const std::vector<float> roundedOutput(roundedView.begin(), roundedView.end());
                                write(roundedOutput, "-fp32-with-fp16-input.f32");
                                for (int probe = 0; probe < 4; ++probe) {
                                    const char *probeNames[]{"brighter", "darker", "shift-right", "shift-left"};
                                    auto probeInput = input;
                                    for (size_t channel = 0; channel < 3; ++channel)
                                        for (size_t y = 0; y < 144; ++y)
                                            for (size_t x = 0; x < 256; ++x) {
                                                const size_t index = channel * pixels + y * 256 + x;
                                                if (probe < 2)
                                                    probeInput[index] = std::clamp(input[index] + (probe == 0 ? 1.0F : -1.0F) / 255.0F, 0.0F, 1.0F);
                                                else {
                                                    const size_t sourceX = probe == 2 ? (x ? x - 1 : 0) : std::min(x + 1, size_t(255));
                                                    probeInput[index] = input[channel * pixels + y * 256 + sourceX];
                                                }
                                            }
                                    const LearningModelBinding probeBinding(session);
                                    probeBinding.Bind(model.InputFeatures().GetAt(0).Name(), TensorFloat::CreateFromArray(winrt::single_threaded_vector<int64_t>({1, 3, 144, 256}), probeInput));
                                    const auto probeResult = session.Evaluate(probeBinding, L"FP16 sensitivity probe");
                                    const auto probeView = probeResult.Outputs().Lookup(model.OutputFeatures().GetAt(0).Name()).as<TensorFloat>().GetAsVectorView();
                                    const std::vector<float> probeReference(probeView.begin(), probeView.end());
                                    std::vector<float> probeActual(pixels);
                                    runLandscapeReference(*runtime, probeInput, probeActual);
                                    write(probeReference, (std::string("-") + probeNames[probe] + "-reference.f32").c_str());
                                    write(probeActual, (std::string("-") + probeNames[probe] + "-actual.f32").c_str());
                                }


                            }
                        }

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
							  << ",rmse=" << std::sqrt(squares / pixels) << ",bad=" << bad << ",mask_disagreements=" << maskDisagreements
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

#ifdef DML_MODEL_DIAGNOSTICS
// Opt-in paired experiment; both graphs differ only at Node 230.
TEST_CASE("MediaPipeLandscapeNode230Timing")
{
	using Clock = std::chrono::steady_clock;
	const auto graph = BackgroundRemoval::Generated::mediaPipeLandscapeDiagnosticGraph();
	const auto weights = BackgroundRemoval::Generated::mediaPipeLandscapeDiagnosticWeights();
	std::vector<Node> nodes(graph.nodes, graph.nodes + graph.nodeCount);
	const auto *fused = std::get_if<FusedNode>(&nodes.at(230));
	REQUIRE(fused != nullptr);
	const auto *specialized = std::get_if<MPLDDepthwiseConv2dRelu>(fused);
	REQUIRE(specialized != nullptr);
	const auto node = *specialized;
	nodes[230] = FusedNode{BiasedConv2dRelu{node.dataType, node.inputIndex, node.filterIndex,
		node.biasIndex, {1, 1}, {1, 1}, {1, 1}, {1, 1}, 16, node.shape}};
	auto nativeGraph = graph;
	nativeGraph.nodes = nodes.data();
	BaselineDmlProgramRuntime native(nativeGraph, weights), candidate(graph, weights);
	const auto input = portraitInput();
	std::vector<float> a(144 * 256), b(a.size());
	const std::array inputs{std::as_bytes(std::span(input))};
	for (int i = 0; i < 20; ++i) {
		native.run(inputs, std::as_writable_bytes(std::span(a)));
		candidate.run(inputs, std::as_writable_bytes(std::span(b)));
	}
	for (size_t i = 0; i < a.size(); ++i)
		REQUIRE((std::isfinite(b[i]) && std::abs(a[i] - b[i]) <= 2e-5F + 2e-5F * std::abs(a[i])));
	std::array<std::vector<double>, 2> samples;
	for (int i = 0; i < 200; ++i)
		for (int order = 0; order < 2; ++order) {
			const int method = (i + order) % 2;
			auto &runtime = method ? candidate : native;
			auto &output = method ? b : a;
			const auto before = Clock::now();
			runtime.run(inputs, std::as_writable_bytes(std::span(output)));
			samples[method].push_back(std::chrono::duration<double, std::milli>(Clock::now() - before).count());
		}
	std::array<wchar_t, 4096> path{};
	const DWORD length = GetEnvironmentVariableW(L"DML_TIMING_CSV", path.data(), static_cast<DWORD>(path.size()));
	if (length && length < path.size()) {
		std::ofstream csv{std::filesystem::path(path.data())};
		csv.exceptions(std::ios::badbit | std::ios::failbit);
		csv << "iteration,native_ms,strip_ms\n";
		for (size_t i = 0; i < samples[0].size(); ++i)
			csv << i << ',' << samples[0][i] << ',' << samples[1][i] << '\n';
	}
	for (int method = 0; method < 2; ++method) {
		auto values = samples[method];
		std::sort(values.begin(), values.end());
		double sum = 0;
		for (const auto value : values)
			sum += value;
		std::cout << "NODE230_TIMING method=" << (method ? "strip" : "native")
			<< ",samples=" << values.size() << ",mean_ms=" << sum / values.size()
			<< ",median_ms=" << (values[99] + values[100]) / 2 << ",p95_ms=" << values[189] << std::endl;
	}
}
#endif

// Keep WinML reference GPU work out of captures of the model runtime.
TEST_CASE("MediaPipeLandscapePixWorkload" * doctest::skip())
{
	auto runtime = BackgroundRemoval::Generated::makeMediaPipeLandscapeDmlProgram();
	const auto input = portraitInput();
	std::vector<float> output(144 * 256);
	for (int i = 0; i < 10; ++i)
		runLandscapeReference(*runtime, input, output);
	REQUIRE(std::all_of(output.begin(), output.end(), [](float value) { return std::isfinite(value); }));
}

// Opt-in timing experiment; intentionally not registered with CTest.
TEST_CASE("MediaPipeLandscapeTiming")
{
	using Clock = std::chrono::steady_clock;
	const auto started = Clock::now();
	auto runtime = BackgroundRemoval::Generated::makeMediaPipeLandscapeDmlProgram();
	const double initialization = std::chrono::duration<double, std::milli>(Clock::now() - started).count();
	const auto input = portraitInput();
	std::vector<float> output(144 * 256);
	const std::array inputs{std::as_bytes(std::span(input))};
	for (int i = 0; i < 10; ++i)
		runLandscapeReference(*runtime, input, output);
	std::vector<double> samples;
	double sum = 0;
	for (int i = 0; i < 200; ++i) {
		const auto before = Clock::now();
		runLandscapeReference(*runtime, input, output);
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

TEST_CASE("MPLDDepthwiseConv2dRelu")
{
	compareOperator("MPLDDepthwiseConv2dRelu");
}

TEST_CASE("MPLDDepthwiseConv2dReluContract")
{
	std::array<InputSpecItem, 3> specs{{
		{kRuntimeInput, dtFloat32, 4, {1, 16, 72, 128}},
		{0, dtFloat32, 4, {16, 1, 3, 3}},
		{576, dtFloat32, 1, {16}},
	}};
	const std::array<float, 160> weights{};
	std::array<Node, 4> nodes{InputItem{dtFloat32, 0}, InputItem{dtFloat32, 1}, InputItem{dtFloat32, 2},
		FusedNode{MPLDDepthwiseConv2dRelu{dtFloat32, 0, 1, 2}}};
	const Graph graph{specs.size(), specs.data(), static_cast<int64_t>(nodes.size()), nodes.data(), 3};
	SUBCASE("input shape") { specs[0].shape[3] = 127; }
	SUBCASE("filter shape") { specs[1].shape[2] = 2; }
	SUBCASE("bias shape") { specs[2].shape[0] = 15; }
	SUBCASE("runtime filter") { specs[1].byteOffset = kRuntimeInput; }
	SUBCASE("input type") { specs[0].dataType = dtFloat16; nodes[0] = InputItem{dtFloat16, 0}; }
	CHECK_THROWS_AS(BaselineDmlProgramRuntime(graph, std::as_bytes(std::span(weights))), std::runtime_error);
}

TEST_CASE("ClipFloat32Contract")
{
	InputSpecItem spec{kRuntimeInput, dtFloat32, 1, {3}};
	std::array<Node, 2> nodes{InputItem{dtFloat32, 0}, Onnx13Node{ClipFloat32{0, 1, {3}, 0, 6}}};
	const Graph graph{1, &spec, 2, nodes.data(), 1};
	SUBCASE("Float16 input") { spec.dataType = dtFloat16; nodes[0] = InputItem{dtFloat16, 0}; }
	SUBCASE("integer input") { spec.dataType = dtInt32; nodes[0] = InputItem{dtInt32, 0}; }
	SUBCASE("reversed bounds") { nodes[1] = Onnx13Node{ClipFloat32{0, 1, {3}, 6, 0}}; }
	CHECK_THROWS_AS(BaselineDmlProgramRuntime(graph, {}), std::runtime_error);
}
