// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#include "LandscapeTestModel.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

// Offline generator validation only. Not a benchmark or a CTest dependency.
int main(int argc, char **argv)
{
	try {
		if (argc != 4)
			throw std::runtime_error("Usage: LandscapeModelCheck input.f32 raw.f32 mask.f32");
		constexpr std::size_t inputCount = 3 * 144 * 256;
		constexpr std::size_t outputCount = 144 * 256;
		if (std::filesystem::file_size(argv[1]) != inputCount * sizeof(float))
			throw std::runtime_error("Expected a contiguous FP32 NCHW RGB input");
		std::vector<float> input(inputCount), output(outputCount);
		std::ifstream inputFile(argv[1], std::ios::binary);
		if (!inputFile.read(reinterpret_cast<char *>(input.data()), input.size() * sizeof(float)))
			throw std::runtime_error("Cannot read input");
		const auto writeOutput = [&](const char *path) {
			std::ofstream stream(path, std::ios::binary);
			if (!stream.write(reinterpret_cast<const char *>(output.data()), output.size() * sizeof(float)))
				throw std::runtime_error("Cannot write output");
		};
		const auto bytes = std::as_bytes(std::span(input));
		const float threshold = 0.42F;
		const std::array rawInputs{bytes, std::as_bytes(std::span(&threshold, 1))};
		auto rawParams = BackgroundRemoval::MPLD10Detail::modelParams;
		rawParams.outputTensor = LandscapeTestModel::rawOutputIndex;
		auto raw = std::make_unique<DmlProgramTest::ProgramTestSession>(
			BackgroundRemoval::MPLD10Detail::graphNodes, rawParams, LandscapeTestModel::weights());
		raw->run(rawInputs, std::as_writable_bytes(std::span(output)));
		writeOutput(argv[2]);
		raw.reset();
		const std::array maskInputs{bytes, std::as_bytes(std::span(&threshold, 1))};
		auto mask = LandscapeTestModel::makeMaskRuntime();
		mask->run(maskInputs, std::as_writable_bytes(std::span(output)));
		writeOutput(argv[3]);
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
