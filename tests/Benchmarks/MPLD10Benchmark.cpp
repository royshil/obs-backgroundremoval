// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <TraceLoggingProvider.h>
#include <benchmark/benchmark.h>
#include "../DmlProgram/LandscapeTestModel.hpp"
#include <array>
#include <iostream>
#include <vector>

TRACELOGGING_DECLARE_PROVIDER(g_hProvider);

static void landscape(benchmark::State &state)
{
	auto runtime = LandscapeTestModel::makeMaskRuntime();
	const std::vector<float> input(3 * 256 * 144, 0.5F);
	const float threshold = 0.42F;
	const std::array inputs{std::as_bytes(std::span(input)), std::as_bytes(std::span(&threshold, 1))};
	std::vector<float> output(256 * 144);
	// Warm up before timing; run() waits for GPU completion and reads back the mask.
	runtime->run(inputs, std::as_writable_bytes(std::span(output)));
	for (auto _ : state) {
		runtime->run(inputs, std::as_writable_bytes(std::span(output)));
		benchmark::DoNotOptimize(output.data());
		benchmark::ClobberMemory();
	}
	state.SetItemsProcessed(state.iterations());
}

int main(int argc, char **argv)
{
	if (TraceLoggingRegister(g_hProvider) != ERROR_SUCCESS)
		return 1;
	int result = 0;
	try {
		benchmark::Initialize(&argc, argv);
		if (benchmark::ReportUnrecognizedArguments(argc, argv))
			result = 1;
		else {
			benchmark::RegisterBenchmark("Landscape/UploadInferenceReadback", landscape)
				->UseRealTime()
				->Unit(benchmark::kMillisecond);
			benchmark::RunSpecifiedBenchmarks();
		}
		benchmark::Shutdown();
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		result = 1;
	}
	TraceLoggingUnregister(g_hProvider);
	return result;
}
