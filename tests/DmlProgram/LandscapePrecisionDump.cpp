// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
// SPDX-License-Identifier: GPL-3.0-or-later
// Opt-in numerical diagnosis. Slow prefix replay intentionally avoids changing runtime execution.
#include "DmlProgram/BaselineDmlProgramRuntime.hpp"
#include <DirectXPackedVector.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <iomanip>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
namespace BackgroundRemoval::Generated {
auto mediaPipeLandscapeDiagnosticGraph() -> BaselineDml::Graph;
auto mediaPipeLandscapeDiagnosticWeights() -> std::span<const std::byte>;
} // namespace BackgroundRemoval::Generated
namespace LandscapePrecisionDump {
using namespace BaselineDml;
using namespace BaselineDml::ProgramDefinition;
auto count(std::span<const std::uint32_t> shape) -> std::size_t
{
	std::size_t n = 1;
	for (auto d : shape)
		n *= d;
	return n;
}
auto read(const std::filesystem::path &path) -> std::vector<float>
{
	std::ifstream f(path, std::ios::binary | std::ios::ate);
	f.exceptions(std::ios::badbit | std::ios::failbit);
	auto bytes = static_cast<std::size_t>(f.tellg());
	if (bytes % 4)
		throw std::runtime_error("Unaligned input file");
	std::vector<float> result(bytes / 4);
	f.seekg(0);
	f.read(reinterpret_cast<char *>(result.data()), bytes);
	return result;
}
void write(const std::filesystem::path &path, std::span<const float> values)
{
	std::ofstream f(path, std::ios::binary);
	f.exceptions(std::ios::badbit | std::ios::failbit);
	f.write(reinterpret_cast<const char *>(values.data()), values.size_bytes());
}
auto pack(std::span<const float> values, std::uint32_t type) -> std::vector<std::byte>
{
	std::vector<std::byte> bytes(values.size() * (type == dtFloat16 ? 2 : 4));
	if (type == dtFloat32)
		std::memcpy(bytes.data(), values.data(), bytes.size());
	else
		for (std::size_t i = 0; i < values.size(); ++i) {
			auto h = DirectX::PackedVector::XMConvertFloatToHalf(values[i]);
			std::memcpy(bytes.data() + 2 * i, &h, 2);
		}
	return bytes;
}
auto unpack(std::span<const std::byte> bytes, std::uint32_t type) -> std::vector<float>
{
	std::vector<float> values(bytes.size() / (type == dtFloat16 ? 2 : 4));
	if (type == dtFloat32)
		std::memcpy(values.data(), bytes.data(), bytes.size());
	else
		for (std::size_t i = 0; i < values.size(); ++i) {
			std::uint16_t h;
			std::memcpy(&h, bytes.data() + 2 * i, 2);
			values[i] = DirectX::PackedVector::XMConvertHalfToFloat(h);
		}
	return values;
}
struct Model {
	std::vector<InputSpecItem> specs;
	std::vector<Node> nodes;
	std::vector<std::byte> weights;
	Model(Graph source, std::uint32_t type)
	{
		nodes.assign(source.nodes, source.nodes + source.nodeCount);
		for (auto &node : nodes)
			std::visit(
				[&](auto &group) {
					if constexpr (std::is_same_v<std::decay_t<decltype(group)>, InputItem>)
						group.dataType = static_cast<std::int32_t>(type);
					else
						std::visit(
							[&](auto &op) {
								if constexpr (std::is_same_v<std::decay_t<decltype(op)>, ClipFloat32>) {
									if (type != dtFloat32)
										throw std::runtime_error("ClipFloat32 cannot be converted to another precision");
								} else
									op.dataType = static_cast<std::int32_t>(type);
							},
							group);
				},
				node);
		auto original = BackgroundRemoval::Generated::mediaPipeLandscapeDiagnosticWeights();
		for (const auto &spec : std::span(source.inputSpecs, source.inputSpecCount)) {
			if (spec.dataType != dtFloat16 && spec.dataType != dtFloat32)
				throw std::runtime_error("Precision dump requires floating-point model tensors");
			auto copy = spec;
			copy.dataType = type;
			if (copy.byteOffset != kRuntimeInput) {
				auto size = count(std::span(spec.shape).first(spec.shapeRank));
				auto values = unpack(original.subspan(spec.byteOffset,
								      size * (spec.dataType == dtFloat16 ? 2 : 4)),
						     spec.dataType);
				auto bytes = pack(values, type);
				copy.byteOffset = weights.size();
				weights.insert(weights.end(), bytes.begin(), bytes.end());
			}
			specs.push_back(copy);
		}
	}
	auto graph(Graph source) const -> Graph
	{
		source.inputSpecs = specs.data();
		source.inputSpecCount = specs.size();
		source.nodes = nodes.data();
		return source;
	}
};
auto execute(BaselineDmlProgramRuntime &runtime, const std::vector<std::vector<float>> &inputs) -> std::vector<float>
{
	if (runtime.inputCount() != inputs.size())
		throw std::runtime_error("Diagnostic input count mismatch");
	std::vector<std::vector<std::byte>> packed;
	for (std::size_t i = 0; i < inputs.size(); ++i)
		packed.push_back(pack(inputs[i], runtime.inputInfo(i).type));
	std::vector<std::span<const std::byte>> spans;
	for (const auto &v : packed)
		spans.emplace_back(v);
	std::vector<std::byte> output(runtime.outputInfo().dataBytes);
	runtime.run(spans, output);
	return unpack(output, runtime.outputInfo().type);
}
auto run(Graph graph, std::span<const std::byte> weights, const std::vector<std::vector<float>> &inputs)
	-> std::vector<float>
{
	BaselineDmlProgramRuntime runtime(graph, weights);
	return execute(runtime, inputs);
}
auto shape(const Node &node, std::span<const InputSpecItem> specs) -> std::vector<std::uint32_t>
{
	return std::visit(
		[&](const auto &group) -> std::vector<std::uint32_t> {
			if constexpr (std::is_same_v<std::decay_t<decltype(group)>, InputItem>) {
				const auto &s = specs[group.inputSpecIndex];
				return {s.shape.begin(), s.shape.begin() + s.shapeRank};
			} else
				return std::visit(
					[](const auto &op) -> std::vector<std::uint32_t> {
						if constexpr (requires { op.shapeRank; })
							return {op.shape.begin(), op.shape.begin() + op.shapeRank};
						else
							return {op.shape.begin(), op.shape.end()};
					},
					group);
		},
		node);
}
// Diagnostic traversal only; execution barriers remain in the generated table.
auto inputIndices(const Node &node) -> std::vector<std::int32_t>
{
	return std::visit(
		[](const auto &group) -> std::vector<std::int32_t> {
			if constexpr (std::is_same_v<std::decay_t<decltype(group)>, InputItem>)
				return {};
			else
				return std::visit(
					[](const auto &op) -> std::vector<std::int32_t> {
						if constexpr (requires { op.leftIndex; })
							return {op.leftIndex, op.rightIndex};
						else if constexpr (requires { op.biasIndex; })
							return {op.inputIndex, op.filterIndex, op.biasIndex};
						else
							return {op.inputIndex};
					},
					group);
		},
		node);
}

auto isolate(Graph source, const Model &model, std::size_t index, const std::vector<std::vector<float>> &reference,
	     bool roundedInputs) -> std::vector<float>
{
	auto refs = inputIndices(source.nodes[index]);
	std::vector<Node> nodes;
	std::vector<InputSpecItem> specs;
	std::vector<std::vector<float>> inputs;
	for (std::size_t i = 0; i < refs.size(); ++i) {
		auto producer = refs[i];
		auto dims = shape(source.nodes[producer], model.specs);
		InputSpecItem spec{};
		spec.dataType = model.specs[0].dataType;
		spec.byteOffset = kRuntimeInput;
		spec.shapeRank = dims.size();
		std::copy(dims.begin(), dims.end(), spec.shape.begin());
		if (const auto *item = std::get_if<InputItem>(&source.nodes[producer]);
		    item && model.specs[item->inputSpecIndex].byteOffset >= 0)
			spec = model.specs[item->inputSpecIndex];
		else {
			auto values = reference[producer];
			if (roundedInputs)
				for (auto &v : values)
					v = DirectX::PackedVector::XMConvertHalfToFloat(
						DirectX::PackedVector::XMConvertFloatToHalf(v));
			inputs.push_back(std::move(values));
		}
		specs.push_back(spec);
		nodes.emplace_back(InputItem{static_cast<std::int32_t>(spec.dataType), static_cast<std::int32_t>(i)});
	}
	Node op = source.nodes[index];
	std::visit(
		[&](auto &group) {
			if constexpr (!std::is_same_v<std::decay_t<decltype(group)>, InputItem>)
				std::visit(
					[&](auto &n) {
						if constexpr (std::is_same_v<std::decay_t<decltype(n)>, ClipFloat32>) {
							if (model.specs[0].dataType != dtFloat32)
								throw std::runtime_error("ClipFloat32 cannot be converted to another precision");
						} else
							n.dataType = static_cast<std::int32_t>(model.specs[0].dataType);
						if constexpr (requires { n.leftIndex; }) {
							n.leftIndex = 0;
							n.rightIndex = 1;
						} else if constexpr (requires { n.biasIndex; }) {
							n.inputIndex = 0;
							n.filterIndex = 1;
							n.biasIndex = 2;
						} else {
							n.inputIndex = 0;
						}
					},
					group);
		},
		op);
	nodes.push_back(op);
	Graph graph{specs.size(), specs.data(), static_cast<int64_t>(nodes.size()), nodes.data(), nodes.size() - 1};
	return run(graph, model.weights, inputs);
}
// Storage-only benchmark: both variants upload identical FLOAT32 tensors.
int benchmarkWeightStorage(const std::filesystem::path &inputPath, const std::filesystem::path &destination)
{
	std::filesystem::create_directories(destination);
	using Clock = std::chrono::steady_clock;
	auto ms = [](auto start) {
		return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
	};
	auto source = BackgroundRemoval::Generated::mediaPipeLandscapeDiagnosticGraph();
	Model model(source, dtFloat32);
	auto half = pack(unpack(BackgroundRemoval::Generated::mediaPipeLandscapeDiagnosticWeights(), dtFloat32), dtFloat16);
	std::vector<DirectX::PackedVector::HALF> halfValues(half.size() / 2);
	std::memcpy(halfValues.data(), half.data(), half.size());
	auto prepare = [&](bool fp16) {
		std::vector<float> values(model.weights.size() / sizeof(float));
		if (fp16)
			DirectX::PackedVector::XMConvertHalfToFloatStream(values.data(), sizeof(float), halfValues.data(),
							       sizeof(halfValues[0]), halfValues.size());
		else
			std::memcpy(values.data(), model.weights.data(), model.weights.size());
		return values;
	};
	auto expanded = prepare(true);
	if (expanded.size() * sizeof(float) != model.weights.size() ||
	    std::memcmp(expanded.data(), model.weights.data(), model.weights.size()))
		throw std::runtime_error("Weight-storage benchmark requires byte-identical FLOAT32 weights");
	std::ofstream csv(destination / "timings.csv");
	csv << std::setprecision(12) << "phase,storage,block,iteration,ms\n";
	for (unsigned i = 0; i < 2000; ++i)
		for (bool fp16 : (i % 2 ? std::array{true, false} : std::array{false, true})) {
			auto start = Clock::now();
			auto values = prepare(fp16);
			double elapsed = ms(start);
			if (std::memcmp(values.data(), model.weights.data(), model.weights.size()))
				throw std::runtime_error("Prepared weight mismatch");
			csv << "prepare," << (fp16 ? "FP16" : "FP32") << ",0," << i << "," << elapsed << "\n";
		}
	std::vector<float> input = read(inputPath), output(256 * 144), expected;
	const std::array inputs{std::as_bytes(std::span(input))};
	for (unsigned block = 0; block < 8; ++block) {
		bool fp16 = block % 8 == 1 || block % 8 == 2 || block % 8 == 4 || block % 8 == 7; // ABBA, then BAAB.
		auto values = prepare(fp16);
		auto start = Clock::now();
		BaselineDmlProgramRuntime runtime(model.graph(source), std::as_bytes(std::span(values)));
		csv << "initialize," << (fp16 ? "FP16" : "FP32") << "," << block << ",0," << ms(start) << "\n";
		for (unsigned i = 0; i < 20; ++i)
			runtime.run(inputs, std::as_writable_bytes(std::span(output)));
		if (expected.empty())
			expected = output;
		if (output != expected)
			throw std::runtime_error("Weight-storage output mismatch");
		for (unsigned i = 0; i < 150; ++i) {
			start = Clock::now();
			runtime.run(inputs, std::as_writable_bytes(std::span(output)));
			csv << "inference," << (fp16 ? "FP16" : "FP32") << "," << block << "," << i << "," << ms(start) << "\n";
		}
		std::cout << "Completed weight-storage block " << block << " " << (fp16 ? "FP16" : "FP32") << std::endl;
	}
	std::ofstream info(destination / "contract.txt");
	info << "storage_fp16_bytes=" << half.size() << "\nstorage_fp32_bytes=" << model.weights.size()
	     << "\ngpu_weights=FLOAT32 byte-identical\nactivations=FLOAT32\nflags=NONE\n"
	        "input=common FLOAT32\noutput=bit-identical\nprepare=allocation plus CPU conversion or copy\n"
	        "initialize=Runtime construction only; excludes prepare\ninference=CPU wall time including submission, wait, readback\n";
	return 0;
}

int screen(const std::filesystem::path &inputDirectory, const std::filesystem::path &destination)
{
	std::filesystem::create_directories(destination);
	auto source = BackgroundRemoval::Generated::mediaPipeLandscapeDiagnosticGraph();
	Model fp32(source, dtFloat32), half(source, dtFloat16);
	BaselineDmlProgramRuntime reference(fp32.graph(source), fp32.weights),
		candidate(half.graph(source), half.weights);
	std::vector<std::filesystem::path> frames;
	for (const auto &entry : std::filesystem::directory_iterator(inputDirectory))
		if (entry.path().extension() == ".f32")
			frames.push_back(entry.path());
	std::sort(frames.begin(), frames.end());
	std::ofstream csv(destination / "frames.csv");
	csv.exceptions(std::ios::badbit | std::ios::failbit);
	csv << "frame,max_abs,rmse,boundary_rmse,binary_flips,alpha_changed_pixels,alpha_max_levels\n";
	for (const auto &frame : frames) {
		auto input = unpack(pack(read(frame), dtFloat16), dtFloat16);
		auto a = execute(reference, {input}), b = execute(candidate, {input});
		double max = 0, sum = 0, boundarySum = 0;
		std::size_t boundaryCount = 0, flips = 0, alphaChanged = 0;
		int alphaMax = 0;
		for (std::size_t i = 0; i < a.size(); ++i) {
			double e = double(a[i]) - b[i];
			sum += e * e;
			max = std::max(max, std::abs(e));
			if (a[i] > .01F && a[i] < .99F) {
				boundarySum += e * e;
				++boundaryCount;
			}
			flips += (a[i] >= .5F) != (b[i] >= .5F);
			int alphaA = 255 - static_cast<int>(std::clamp((1.0F - a[i]) * 255.0F, 0.0F, 255.0F));
			int alphaB = 255 - static_cast<int>(std::clamp((1.0F - b[i]) * 255.0F, 0.0F, 255.0F));
			int delta = std::abs(alphaA - alphaB);
			alphaChanged += delta != 0;
			alphaMax = std::max(alphaMax, delta);
		}
		csv << frame.filename().string() << "," << max << "," << std::sqrt(sum / a.size()) << ","
		    << std::sqrt(boundarySum / std::max<std::size_t>(boundaryCount, 1)) << "," << flips << ","
		    << alphaChanged << "," << alphaMax << "\n";
		write(destination / (frame.stem().string() + "-fp32.f32"), a);
		write(destination / (frame.stem().string() + "-fp16-none.f32"), b);
		std::cout << "Screened " << frame.filename().string() << std::endl;
	}
	return 0;
}

auto errorRms(std::span<const float> a, std::span<const float> b) -> double
{
	double sum = 0;
	for (std::size_t i = 0; i < a.size(); ++i) {
		double e = double(a[i]) - b[i];
		sum += e * e;
	}
	return std::sqrt(sum / a.size());
}
auto operatorNodes(Graph source) -> std::vector<std::size_t>
{
	std::vector<std::size_t> result;
	for (std::size_t i = 0; i < static_cast<std::size_t>(source.nodeCount); ++i)
		if (!std::holds_alternative<InputItem>(source.nodes[i]))
			result.push_back(i);
	return result;
}
int checkGpuStorage(const std::filesystem::path &inputPath, const std::filesystem::path &destination)
{
	auto source = BackgroundRemoval::Generated::mediaPipeLandscapeDiagnosticGraph();
	Model model(source, dtFloat32);
	BaselineDmlProgramRuntime runtime(model.graph(source), model.weights);
	auto indices = operatorNodes(source);
	std::vector<std::uint8_t> rounded(indices.size(), 1);
	runtime.configureStorageDiagnostics(rounded);
	auto input = unpack(pack(read(inputPath), dtFloat16), dtFloat16);
	auto out = execute(runtime, {input});
	const auto &values = runtime.diagnosticIntermediateValues();
	std::ofstream csv(destination / "gpu-rounding-check.csv");
	csv.exceptions(std::ios::badbit | std::ios::failbit);
	csv << "node,rmse_vs_cpu_storage\n";
	for (std::size_t j = 0; j < indices.size(); ++j) {
		auto expected = read(destination / (std::to_string(indices[j]) + "-storage-after-round.f32"));
		auto error = errorRms(expected, values[j]);
		csv << indices[j] << "," << error << "\n";
		if (error != 0)
			throw std::runtime_error("GPU/CPU storage rounding differs");
	}
	write(destination / "gpu-storage-final.f32", out);
	std::cout << "GPU rounding matches CPU storage at all " << indices.size() << " operators" << std::endl;
	return 0;
}
int selectStorage(const std::filesystem::path &inputDirectory, const std::filesystem::path &destination, bool checkOnly)
{
	std::filesystem::create_directories(destination);
	constexpr double limit = .001;
	auto source = BackgroundRemoval::Generated::mediaPipeLandscapeDiagnosticGraph();
	Model model(source, dtFloat32);
	BaselineDmlProgramRuntime baseline(model.graph(source), model.weights),
		candidate(model.graph(source), model.weights);
	auto indices = operatorNodes(source);
	std::vector<std::uint8_t> zeros(indices.size(), 0);
	baseline.configureStorageDiagnostics(zeros);
	std::vector<bool> keep(source.nodeCount, false);
	if (std::ifstream prior(destination / "keep-fp32.txt"); prior) {
		std::size_t i;
		while (prior >> i)
			keep.at(i) = true;
	}
	std::vector<std::filesystem::path> frames;
	for (const auto &entry : std::filesystem::directory_iterator(inputDirectory))
		if (entry.path().extension() == ".f32")
			frames.push_back(entry.path());
	std::sort(frames.begin(), frames.end());
	if (std::ifstream priorities(inputDirectory / "priority.txt"); priorities) {
		std::vector<std::filesystem::path> ordered;
		std::string name;
		while (priorities >> name) {
			auto f = inputDirectory / name;
			if (std::find(frames.begin(), frames.end(), f) != frames.end() &&
			    std::find(ordered.begin(), ordered.end(), f) == ordered.end())
				ordered.push_back(f);
		}
		for (auto &f : frames)
			if (std::find(ordered.begin(), ordered.end(), f) == ordered.end())
				ordered.push_back(f);
		frames = std::move(ordered);
	}
	// The final projection is now represented by Conv and DepthToSpace.
	std::vector<bool> projectionNodes(source.nodeCount, false);
	for (std::size_t i = 0; i < static_cast<std::size_t>(source.nodeCount); ++i) {
		if (const auto *group = std::get_if<Onnx11Node>(&source.nodes[i])) {
			if (const auto *shuffle = std::get_if<DepthToSpace>(group)) {
				projectionNodes[i] = true;
				projectionNodes.at(shuffle->inputIndex) = true;
			}
		}
	}
	auto exempt = [&](std::size_t node) { return projectionNodes[node]; };
	auto savePolicy = [&] {
		std::ofstream file(destination / "keep-fp32.txt");
		file.exceptions(std::ios::badbit | std::ios::failbit);
		for (auto i : indices)
			if (keep[i])
				file << i << "\n";
	};
	std::ofstream log(destination / (checkOnly ? "check.log.csv" : "promotions.csv"));
	log.exceptions(std::ios::badbit | std::ios::failbit);
	log << "pass,frame,first_violation,promoted_node,rmse_before,rmse_after\n";
	for (unsigned pass = 0; pass < 126; ++pass) {
		bool changed = false;
		std::vector<double> maxima(indices.size(), 0);
		std::vector<std::string> worst(indices.size());
		std::size_t checked = 0;
		std::ofstream perFrame(destination / (checkOnly ? "check-frames.csv" : "validation-frames.csv"));
		perFrame.exceptions(std::ios::badbit | std::ios::failbit);
		perFrame << "frame,max_intermediate_rmse,max_node,final_mask_rmse\n";
		for (const auto &frame : frames) {
			auto input = unpack(pack(read(frame), dtFloat16), dtFloat16);
			execute(baseline, {input});
			const auto &reference = baseline.diagnosticIntermediateValues();
			auto evaluate = [&] {
				std::vector<std::uint8_t> flags;
				for (auto i : indices)
					flags.push_back(!keep[i]);
				candidate.configureStorageDiagnostics(flags);
				execute(candidate, {input});
			};
			auto firstBad = [&]() -> std::size_t {
				const auto &actual = candidate.diagnosticIntermediateValues();
				for (std::size_t j = 0; j < indices.size(); ++j)
					if (!exempt(indices[j]) && !(errorRms(reference[j], actual[j]) <= limit))
						return j;
				return indices.size();
			};
			evaluate();
			for (auto bad = firstBad(); bad != indices.size(); bad = firstBad()) {
				auto before = errorRms(reference[bad], candidate.diagnosticIntermediateValues()[bad]);
				if (checkOnly)
					throw std::runtime_error("Storage policy violates limit at frame " +
								 frame.filename().string() + " node " +
								 std::to_string(indices[bad]));
				std::vector<bool> ancestors(source.nodeCount, false);
				ancestors[indices[bad]] = true;
				for (std::size_t i = indices[bad] + 1; i-- > 0;)
					if (ancestors[i]) {
						auto refs = inputIndices(source.nodes[i]);
						for (std::size_t k = 0; k < refs.size(); ++k)
							ancestors[refs[k]] = true;
					}
				std::size_t best = source.nodeCount;
				double bestError = std::numeric_limits<double>::infinity();
				for (auto i : indices)
					if (ancestors[i] && !keep[i] && !exempt(i)) {
						keep[i] = true;
						evaluate();
						double trial = errorRms(reference[bad],
									candidate.diagnosticIntermediateValues()[bad]);
						keep[i] = false;
						if (trial < bestError) {
							bestError = trial;
							best = i;
						}
					}
				if (best == static_cast<std::size_t>(source.nodeCount))
					throw std::runtime_error("No storage boundary can fix violation");
				keep[best] = true;
				changed = true;
				savePolicy();
				log << pass << "," << frame.filename().string() << "," << indices[bad] << "," << best
				    << "," << before << "," << bestError << "\n";
				log.flush();
				std::cout << "Promoted " << best << " for " << frame.filename().string() << " at "
					  << indices[bad] << ": " << before << " -> " << bestError << std::endl;
				evaluate();
			}
			const auto &actual = candidate.diagnosticIntermediateValues();
			double maximum = 0;
			std::size_t maxNode = 0;
			for (std::size_t j = 0; j < indices.size(); ++j) {
				double error = errorRms(reference[j], actual[j]);
				if (error > maxima[j]) {
					maxima[j] = error;
					worst[j] = frame.filename().string();
				}
				if (!exempt(indices[j]) && error > maximum) {
					maximum = error;
					maxNode = indices[j];
				}
			}
			perFrame << frame.filename().string() << "," << maximum << "," << maxNode << ","
				 << errorRms(reference.back(), actual.back()) << "\n";
			if (++checked % 50 == 0)
				std::cout << "Pass " << pass << " validated " << checked << "/" << frames.size()
					  << std::endl;
		}
		std::ofstream csv(destination / (checkOnly ? "check-node-maxima.csv" : "node-maxima.csv"));
		csv.exceptions(std::ios::badbit | std::ios::failbit);
		csv << "node,fp32_kept,exempt,max_rmse,worst_frame\n";
		for (std::size_t j = 0; j < indices.size(); ++j)
			csv << indices[j] << "," << keep[indices[j]] << "," << exempt(indices[j]) << "," << maxima[j]
			    << "," << worst[j] << "\n";
		std::cout << "Finished pass " << pass << ", kept " << std::count(keep.begin(), keep.end(), true)
			  << ", changed=" << changed << std::endl;
		if (!changed) {
			savePolicy();
			return 0;
		}
	}
	throw std::runtime_error("Storage policy selection did not converge");
}

// Storage-only experiment: every operation is executed with FLOAT32 tensors
// and NONE flags; CPU round trips deliberately model only FP16 storage boundaries.
int storageBoundaries(const std::filesystem::path &inputPath, const std::filesystem::path &destination)
{
	std::filesystem::create_directories(destination);
	auto input = unpack(pack(read(inputPath), dtFloat16), dtFloat16);
	if (input.size() != 3 * 144 * 256)
		throw std::runtime_error("Expected Landscape input");
	auto source = BackgroundRemoval::Generated::mediaPipeLandscapeDiagnosticGraph();
	Model fp32(source, dtFloat32), half(source, dtFloat16);
	std::vector<std::vector<float>> reference(source.nodeCount), rounded(source.nodeCount),
		keepFirst(source.nodeCount);
	std::ofstream trace(destination / "storage-trace.csv");
	trace.exceptions(std::ios::badbit | std::ios::failbit);
	trace << "node,pre_round_max_abs,post_round_max_abs,new_rounding_max_abs\n";
	auto maxError = [](std::span<const float> a, std::span<const float> b) {
		double result = 0;
		for (std::size_t i = 0; i < a.size(); ++i)
			result = std::max(result, std::abs(double(a[i]) - b[i]));
		return result;
	};
	auto rmse = [](std::span<const float> a, std::span<const float> b) {
		double sum = 0;
		for (std::size_t i = 0; i < a.size(); ++i) {
			double e = double(a[i]) - b[i];
			sum += e * e;
		}
		return std::sqrt(sum / a.size());
	};
	for (std::size_t i = 0; i < static_cast<std::size_t>(source.nodeCount); ++i) {
		if (const auto *item = std::get_if<InputItem>(&source.nodes[i])) {
			const auto &spec = fp32.specs[item->inputSpecIndex];
			reference[i] =
				spec.byteOffset == kRuntimeInput
					? input
					: unpack(std::span(fp32.weights)
							 .subspan(spec.byteOffset,
								  count(std::span(spec.shape).first(spec.shapeRank)) *
									  4),
						 dtFloat32);
			rounded[i] = reference[i];
			keepFirst[i] = reference[i];
			continue;
		}
		reference[i] = isolate(source, fp32, i, reference, false);
		auto before = isolate(source, fp32, i, rounded, false);
		rounded[i] = unpack(pack(before, dtFloat16), dtFloat16);
		auto kept = isolate(source, fp32, i, keepFirst, false);
		keepFirst[i] = i == 3 ? kept : unpack(pack(kept, dtFloat16), dtFloat16);
		trace << i << "," << maxError(reference[i], before) << "," << maxError(reference[i], rounded[i]) << ","
		      << maxError(before, rounded[i]) << "\n";
		write(destination / (std::to_string(i) + "-storage-reference.f32"), reference[i]);
		write(destination / (std::to_string(i) + "-storage-before-round.f32"), before);
		write(destination / (std::to_string(i) + "-storage-after-round.f32"), rounded[i]);
		std::cout << "Storage trace node " << i << std::endl;
	}
	const auto baseline = run(fp32.graph(source), fp32.weights, {input});
	const auto fullHalf = run(half.graph(source), half.weights, {input});
	write(destination / "common-input.f32", input);
	write(destination / "baseline-fp32.f32", baseline);
	write(destination / "full-fp16-none.f32", fullHalf);
	write(destination / "storage-only-all-rounded.f32", rounded[source.outputIndex]);
	write(destination / "storage-only-keep-first-conv.f32", keepFirst[source.outputIndex]);
	std::ofstream checks(destination / "checks.csv");
	checks.exceptions(std::ios::badbit | std::ios::failbit);
	checks << "comparison,max_abs,rmse\n";
	checks << "sequential-fp32-vs-full-fp32," << maxError(baseline, reference[source.outputIndex]) << ","
	       << rmse(baseline, reference[source.outputIndex]) << "\n";
	checks << "storage-only-vs-full-fp16-none," << maxError(fullHalf, rounded[source.outputIndex]) << ","
	       << rmse(fullHalf, rounded[source.outputIndex]) << "\n";
	// Replace one producer by an FP32 runtime input carrying only its rounded
	// reference value. All downstream consumers see this value, including branches.
	std::ofstream injection(destination / "single-boundary.csv");
	injection.exceptions(std::ios::badbit | std::ios::failbit);
	injection << "node,final_max_abs,final_rmse\n";
	for (std::size_t i = 0; i < static_cast<std::size_t>(source.nodeCount); ++i) {
		if (std::holds_alternative<InputItem>(source.nodes[i]))
			continue;
		auto dims = shape(source.nodes[i], fp32.specs);
		auto specs = fp32.specs;
		InputSpecItem spec{};
		spec.dataType = dtFloat32;
		spec.byteOffset = kRuntimeInput;
		spec.shapeRank = dims.size();
		std::copy(dims.begin(), dims.end(), spec.shape.begin());
		specs.push_back(spec);
		std::vector<Node> nodes(source.nodes, source.nodes + source.nodeCount);
		nodes[i] = InputItem{dtFloat32, static_cast<std::int32_t>(specs.size() - 1)};
		auto graph = fp32.graph(source);
		graph.inputSpecs = specs.data();
		graph.inputSpecCount = specs.size();
		graph.nodes = nodes.data();
		std::vector<std::array<std::int32_t, 8>> uavBarriers(source.uavBarriers,
								     source.uavBarriers + source.nodeCount);
		uavBarriers[i].fill(-1);
		graph.uavBarriers = uavBarriers.data();
		const auto injected = unpack(pack(reference[i], dtFloat16), dtFloat16);
		auto output = run(graph, fp32.weights, {input, injected});
		injection << i << "," << maxError(baseline, output) << "," << rmse(baseline, output) << "\n";
		write(destination / (std::to_string(i) + "-single-boundary-final.f32"), output);
		std::cout << "Single storage boundary " << i << std::endl;
	}
	return 0;
}

int main(const std::filesystem::path &inputPath, const std::filesystem::path &destination)
{
	std::filesystem::create_directories(destination);
	auto input = read(inputPath);
	if (input.size() != 3 * 144 * 256)
		throw std::runtime_error("Expected planar 256x144 RGB Float32 input");
	auto source = BackgroundRemoval::Generated::mediaPipeLandscapeDiagnosticGraph();
	Model fp32(source, dtFloat32), fp16None(source, dtFloat16);
	std::array<Model *, 2> models{&fp32, &fp16None};
	std::array<const char *, 2> names{"fp32", "fp16-none"};
	std::vector<std::vector<float>> reference(source.nodeCount);
	std::ofstream manifest(destination / "nodes.csv");
	manifest.exceptions(std::ios::badbit | std::ios::failbit);
	manifest << "node,shape,inputs\n";
	for (std::size_t index = 0; index < static_cast<std::size_t>(source.nodeCount); ++index) {
		if (const auto *item = std::get_if<InputItem>(&source.nodes[index])) {
			const auto &spec = fp32.specs[item->inputSpecIndex];
			reference[index] =
				spec.byteOffset == kRuntimeInput
					? input
					: unpack(std::span(fp32.weights)
							 .subspan(spec.byteOffset,
								  count(std::span(spec.shape).first(spec.shapeRank)) *
									  4),
						 dtFloat32);
			continue;
		}
		auto dims = shape(source.nodes[index], fp32.specs);
		manifest << index << ",\"";
		for (auto d : dims)
			manifest << d << " ";
		manifest << "\",\"";
		auto refs = inputIndices(source.nodes[index]);
		for (std::size_t i = 0; i < refs.size(); ++i)
			manifest << refs[i] << " ";
		manifest << "\"\n";
		for (std::size_t variant = 0; variant < models.size(); ++variant) {
			auto graph = models[variant]->graph(source);
			graph.nodeCount = index + 1;
			graph.outputIndex = index;
			auto values = run(graph, models[variant]->weights, {input});
			write(destination / (std::to_string(index) + "-prefix-" + names[variant] + ".f32"), values);
			if (!variant)
				reference[index] = std::move(values);
		}
		for (std::size_t variant = 0; variant < models.size(); ++variant)
			write(destination / (std::to_string(index) + "-local-" + names[variant] + ".f32"),
			      isolate(source, *models[variant], index, reference, false));
		write(destination / (std::to_string(index) + "-local-fp32-rounded.f32"),
		      isolate(source, fp32, index, reference, true));
		std::cout << "Dumped node " << index << "/" << source.nodeCount - 1 << std::endl;
	}
	write(destination / "input.f32", input);
	return 0;
}
} // namespace LandscapePrecisionDump
int main(int argc, char **argv)
{
	try {
		if (argc != 3 && argc != 4)
			throw std::runtime_error(
				"Usage: LandscapePrecisionDump planar-input.f32 output-directory [mode]");
		if (argc == 4 && std::string_view(argv[3]) == "--weight-storage-benchmark")
			return LandscapePrecisionDump::benchmarkWeightStorage(argv[1], argv[2]);
		if (argc == 4 && std::string_view(argv[3]) == "--gpu-storage-check")
			return LandscapePrecisionDump::checkGpuStorage(argv[1], argv[2]);
		if (argc == 4 &&
		    (std::string_view(argv[3]) == "--policy-select" || std::string_view(argv[3]) == "--policy-check"))
			return LandscapePrecisionDump::selectStorage(argv[1], argv[2],
								     std::string_view(argv[3]) == "--policy-check");
		if (argc == 4 && std::string_view(argv[3]) == "--screen")
			return LandscapePrecisionDump::screen(argv[1], argv[2]);
		if (argc == 4 && std::string_view(argv[3]) == "--storage-boundaries")
			return LandscapePrecisionDump::storageBoundaries(argv[1], argv[2]);
		if (argc == 4)
			throw std::invalid_argument("Unknown diagnostic mode");
		return LandscapePrecisionDump::main(argv[1], argv[2]);
	} catch (const std::exception &e) {
		std::cerr << e.what() << "\n";
		return 1;
	}
}
