// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <array>
#include <iostream>
#include <vector>

#include <TraceLoggingProvider.h>

#include <benchmark/benchmark.h>

#include <BackgroundRemoval/windows/MediaPipeLandscapeDirectML10/MPLD10Model.hpp>
#include "../DmlProgram/ProgramTestSession.hpp"

TRACELOGGING_DECLARE_PROVIDER(g_hProvider);

static void landscape(benchmark::State &state)
{
	const auto weights = std::as_bytes(std::span(BackgroundRemoval::MPLD10Detail::weightsData,
						     BackgroundRemoval::MPLD10Detail::weightsDataSize));
	DmlProgramTest::ProgramTestSession runtime(BackgroundRemoval::MPLD10Detail::graphNodes,
						   BackgroundRemoval::MPLD10Detail::modelParams, weights);
	const std::vector<float> input(3 * 256 * 144, 0.5F);
	const float threshold = 0.42F;
	const std::array inputs{std::as_bytes(std::span(input)), std::as_bytes(std::span(&threshold, 1))};
	std::vector<float> output(256 * 144);
	// Warm up before timing; run() waits for GPU completion and reads back the mask.
	runtime.run(inputs, std::as_writable_bytes(std::span(output)));
	for (auto _ : state) {
		runtime.run(inputs, std::as_writable_bytes(std::span(output)));
		benchmark::DoNotOptimize(output.data());
		benchmark::ClobberMemory();
	}
	state.SetItemsProcessed(state.iterations());
}

int main(int argc, char **argv)
{
	if (TraceLoggingRegister(g_hProvider) != ERROR_SUCCESS) {
		return 1;
	}

	struct TraceLoggingCleanup {
		~TraceLoggingCleanup() { TraceLoggingUnregister(g_hProvider); }
	} traceLoggingCleanup;

	try {
		benchmark::Initialize(&argc, argv);
		struct BenchmarkCleanup {
			~BenchmarkCleanup() { benchmark::Shutdown(); }
		} benchmarkCleanup;
		if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
			return 1;
		}
		benchmark::RegisterBenchmark("Landscape/UploadInferenceReadback", landscape)
			->UseRealTime()
			->Unit(benchmark::kMillisecond);
		benchmark::RunSpecifiedBenchmarks();
	} catch (const std::exception &error) {
		std::cerr << error.what() << std::endl;
		return 1;
	}

	return 0;
}
