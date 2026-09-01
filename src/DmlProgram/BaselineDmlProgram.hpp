// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace BaselineDml {

using Index = std::uint32_t;
constexpr Index noIndex = UINT32_MAX;
constexpr std::size_t maximumRank = 4;
constexpr std::size_t maximumConstants = 2;

struct Dimensions final {
	std::uint32_t count;
	std::array<std::uint32_t, maximumRank> values;
};

struct Tensor final {
	Dimensions dimensions;
	Index weightOffset = noIndex;
};

enum class NodeType : std::uint8_t {
	reshape,
	transpose,
	add,
	multiply,
	clip,
	relu,
	sigmoid,
	convolution,
	averagePool,
	gemm,
	upsample,
	join,
};
enum class ConvolutionDirection : std::uint8_t { forward, backward };

struct ConstantInput final {
	std::uint32_t inputIndex;
	Tensor tensor;
};

struct Node final {
	NodeType type;
	Tensor output;
	std::uint32_t constantCount;
	std::array<ConstantInput, maximumConstants> constants;
	ConvolutionDirection direction;
	Dimensions first;
	Dimensions second;
	Dimensions third;
	Dimensions fourth;
	float minimum;
	float maximum;
	std::uint32_t value;
};

struct InputEdge final {
	Index graphInput;
	Index toNode;
	Index toNodeInput;
};
struct IntermediateEdge final {
	Index fromNode;
	Index fromNodeOutput;
	Index toNode;
	Index toNodeInput;
};
struct OutputEdge final {
	Index fromNode;
	Index fromNodeOutput;
	Index graphOutput;
};

struct Graph final {
	std::uint32_t inputCount;
	const Tensor *inputs;
	std::uint32_t outputCount;
	std::uint32_t nodeCount;
	const Node *nodes;
	std::uint32_t inputEdgeCount;
	const InputEdge *inputEdges;
	std::uint32_t intermediateEdgeCount;
	const IntermediateEdge *intermediateEdges;
	std::uint32_t outputEdgeCount;
	const OutputEdge *outputEdges;
};

} // namespace BaselineDml

namespace BackgroundRemoval {

class BaselineDmlProgram final {
public:
	explicit BaselineDmlProgram(const BaselineDml::Graph &graph, std::span<const std::byte> weights);
	~BaselineDmlProgram() noexcept;

	BaselineDmlProgram(const BaselineDmlProgram &) = delete;
	auto operator=(const BaselineDmlProgram &) -> BaselineDmlProgram & = delete;
	BaselineDmlProgram(BaselineDmlProgram &&) = delete;
	auto operator=(BaselineDmlProgram &&) -> BaselineDmlProgram & = delete;

	void run(std::span<const float> input, std::span<float> output);

private:
	class Impl;
	std::unique_ptr<Impl> impl_;
};

} // namespace BackgroundRemoval
