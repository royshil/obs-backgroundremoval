// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <variant>

namespace BaselineDml {

using UINT = std::uint32_t;

using InputSpec = std::variant<std::array<UINT, 1>, std::array<UINT, 2>, std::array<UINT, 3>, std::array<UINT, 4>>;

struct Input final {
	std::size_t inputSpecIndex;
};

struct Constant final {
	std::int64_t byteOffset;
	std::size_t shapeRank;
	std::array<UINT, 4> shape;
};
struct Reshape final {
	std::size_t inputIndex;
	std::size_t shapeRank;
	std::array<UINT, 4> shape;
};
struct TransposeRank2 final {
	std::size_t inputIndex;
	std::array<UINT, 2> permutation;
};
struct TransposeRank3 final {
	std::size_t inputIndex;
	std::array<UINT, 3> permutation;
};
struct TransposeRank4 final {
	std::size_t inputIndex;
	std::array<UINT, 4> permutation;
};
struct AddRank4 final {
	std::size_t leftIndex;
	std::size_t rightIndex;
	std::array<UINT, 4> shape;
};
struct MulRank4 final {
	std::size_t leftIndex;
	std::size_t rightIndex;
	std::array<UINT, 4> shape;
};
struct Clip final {
	std::size_t inputIndex;
	float minimum;
	float maximum;
};
struct Relu final {
	std::size_t inputIndex;
};
struct Sigmoid final {
	std::size_t inputIndex;
};
struct Conv2d final {
	std::size_t inputIndex;
	std::size_t filterIndex;
	std::array<UINT, 2> strides;
	std::array<UINT, 2> dilations;
	std::array<UINT, 2> startPadding;
	std::array<UINT, 2> endPadding;
	UINT groupCount;
	std::array<UINT, 4> shape;
};
struct BiasedConv2d final {
	std::size_t inputIndex;
	std::size_t filterIndex;
	std::size_t biasIndex;
	std::array<UINT, 2> strides;
	std::array<UINT, 2> dilations;
	std::array<UINT, 2> startPadding;
	std::array<UINT, 2> endPadding;
	UINT groupCount;
	std::array<UINT, 4> shape;
};
struct ConvTranspose2d final {
	std::size_t inputIndex;
	std::size_t filterIndex;
	std::array<UINT, 2> strides;
	std::array<UINT, 2> dilations;
	std::array<UINT, 2> startPadding;
	std::array<UINT, 2> endPadding;
	UINT groupCount;
	std::array<UINT, 4> shape;
};
struct BiasedConvTranspose2d final {
	std::size_t inputIndex;
	std::size_t filterIndex;
	std::size_t biasIndex;
	std::array<UINT, 2> strides;
	std::array<UINT, 2> dilations;
	std::array<UINT, 2> startPadding;
	std::array<UINT, 2> endPadding;
	UINT groupCount;
	std::array<UINT, 4> shape;
};
struct AveragePool2d final {
	std::size_t inputIndex;
	std::array<UINT, 2> strides;
	std::array<UINT, 2> window;
	std::array<UINT, 2> startPadding;
	std::array<UINT, 2> endPadding;
	std::array<UINT, 4> shape;
};
struct MatMulRank4 final {
	std::size_t leftIndex;
	std::size_t rightIndex;
	std::array<UINT, 4> shape;
};
struct ResizeRank4 final {
	std::size_t inputIndex;
	UINT height;
	UINT width;
	std::array<UINT, 4> shape;
};
struct Concat final {
	std::size_t leftIndex;
	std::size_t rightIndex;
	UINT axis;
	std::array<UINT, 4> shape;
};

using Node = std::variant<Input, Constant, Reshape, TransposeRank2, TransposeRank3, TransposeRank4, AddRank4, MulRank4,
			  Clip, Relu, Sigmoid, Conv2d, BiasedConv2d, ConvTranspose2d, BiasedConvTranspose2d,
			  AveragePool2d, MatMulRank4, ResizeRank4, Concat>;

struct Graph final {
	std::size_t inputSpecCount;
	const InputSpec *inputSpecs;
	std::int64_t nodeCount;
	const Node *nodes;
	std::size_t outputIndex;
};

class BaselineDmlProgram final {
public:
	explicit BaselineDmlProgram(const Graph &graph, std::span<const std::byte> weights);
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

} // namespace BaselineDml
