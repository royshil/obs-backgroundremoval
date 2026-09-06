// SPDX-FileCopyrightText: 2026 Xavier Ruiz <github@xav.ie>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <opencv2/core.hpp>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <numeric>
#include <string>
#include <thread>
#include <vector>
#include <mutex>

#ifdef __linux__
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

#ifndef _WIN32
#include <sys/resource.h>
#endif

#ifdef HAS_ONNXRUNTIME
#include <onnxruntime_cxx_api.h>
#endif

#include "pipeline-helpers.h"

using Clock = std::chrono::high_resolution_clock;
using std::chrono::duration;

// ─── Hardware performance counters (Linux only) ──────────────────────

struct PerfCounter {
	int fd = -1;
	const char *name;

#ifdef __linux__
	PerfCounter(uint32_t type, uint64_t config, const char *label) : name(label)
	{
		struct perf_event_attr pe = {};
		pe.type = type;
		pe.size = sizeof(pe);
		pe.config = config;
		pe.disabled = 1;
		pe.exclude_kernel = 1;
		pe.exclude_hv = 1;
		fd = (int)syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0);
	}
	~PerfCounter()
	{
		if (fd >= 0)
			close(fd);
	}
	void reset()
	{
		if (fd >= 0)
			ioctl(fd, PERF_EVENT_IOC_RESET, 0);
	}
	void enable()
	{
		if (fd >= 0)
			ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
	}
	void disable()
	{
		if (fd >= 0)
			ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
	}
	long long read_count()
	{
		if (fd < 0)
			return -1;
		long long count = 0;
		if (::read(fd, &count, sizeof(count)) != sizeof(count))
			return -1;
		return count;
	}
#else
	PerfCounter(uint32_t, uint64_t, const char *label) : name(label) {}
	~PerfCounter() {}
	void reset() {}
	void enable() {}
	void disable() {}
	long long read_count() { return -1; }
#endif
};

// ─── CPU time measurement ────────────────────────────────────────────

#ifndef _WIN32
static double get_cpu_seconds()
{
	struct rusage ru;
	getrusage(RUSAGE_SELF, &ru);
	return (ru.ru_utime.tv_sec + ru.ru_utime.tv_usec / 1e6) + (ru.ru_stime.tv_sec + ru.ru_stime.tv_usec / 1e6);
}
#endif

// ─── Pipeline benchmark ──────────────────────────────────────────────

// Exercises the same pre-inference pipeline the plugin uses in video_tick:
// 1. Clone the input frame          (simulates inputBGRA.clone())
// 2. Resize to 192x108 thumbnail    (pipeline::preprocess_resize)
// 3. pipeline::check_similarity     (PSNR skip check on thumbnail)
static double process_frame(cv::Mat &lastImage, std::mutex &mtx, const cv::Mat &frame)
{
	auto t0 = Clock::now();

	// Step 1: Clone under lock (simulates video_tick acquiring inputBGRA)
	cv::Mat cloned;
	{
		std::lock_guard<std::mutex> lock(mtx);
		cloned = frame.clone();
	}

	// Step 2: Downsample to thumbnail for fast PSNR comparison
	cv::Mat thumbnail = pipeline::preprocess_resize(cloned, 192, 108);

	// Step 3: Similarity check on thumbnail (~81KB vs ~7.9MB)
	pipeline::check_similarity(thumbnail, lastImage, 35.0);

	auto t1 = Clock::now();
	return duration<double, std::micro>(t1 - t0).count();
}

// Generate synthetic 1920x1080 BGRA frames.
// Most frames differ by small noise (exercises PSNR skip path);
// every 30th frame is a "scene change" (exercises the non-skip path).
static std::vector<cv::Mat> generate_frames(int count)
{
	const int W = 1920, H = 1080;
	std::vector<cv::Mat> frames;
	frames.reserve(count);

	cv::RNG rng(42);
	cv::Mat base(H, W, CV_8UC4);
	rng.fill(base, cv::RNG::UNIFORM, 0, 256);

	for (int i = 0; i < count; i++) {
		if (i > 0 && i % 30 == 0) {
			rng.fill(base, cv::RNG::UNIFORM, 0, 256);
		} else if (i > 0) {
			cv::Mat noise(H, W, CV_8UC4);
			rng.fill(noise, cv::RNG::UNIFORM, 0, 4);
			cv::add(base, noise, base);
		}
		frames.push_back(base.clone());
	}
	printf("Generated %d synthetic frames (%dx%d BGRA)\n", count, W, H);
	return frames;
}

int main(int argc, char **argv)
{
	std::string model_path;
	int max_frames = 660;

	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i];
		if (arg == "-n" && i + 1 < argc)
			max_frames = std::stoi(argv[++i]);
		else if (arg == "-m" && i + 1 < argc)
			model_path = argv[++i];
	}

	auto frames = generate_frames(max_frames);

	size_t frame_bytes = frames[0].total() * frames[0].elemSize();

	// Set up hardware performance counters (Linux perf_event_open)
#ifdef __linux__
	PerfCounter cache_refs(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_REFERENCES, "cache-references");
	PerfCounter cache_misses(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES, "cache-misses");
	PerfCounter llc_stores(PERF_TYPE_HW_CACHE,
			       PERF_COUNT_HW_CACHE_LL | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
				       (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16),
			       "LLC-stores");
	PerfCounter llc_store_misses(PERF_TYPE_HW_CACHE,
				     PERF_COUNT_HW_CACHE_LL | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
					     (PERF_COUNT_HW_CACHE_RESULT_MISS << 16),
				     "LLC-store-misses");
	bool has_perf = (cache_misses.fd >= 0);
#else
	PerfCounter cache_refs(0, 0, "cache-references");
	PerfCounter cache_misses(0, 0, "cache-misses");
	PerfCounter llc_stores(0, 0, "LLC-stores");
	PerfCounter llc_store_misses(0, 0, "LLC-store-misses");
	bool has_perf = false;
#endif

	// Warmup (10 frames)
	{
		std::mutex mtx;
		cv::Mat lastImage;
		for (int i = 0; i < 10 && i < (int)frames.size(); i++)
			process_frame(lastImage, mtx, frames[i]);
	}

	// Reset and enable counters before benchmark
	cache_refs.reset();
	cache_misses.reset();
	llc_stores.reset();
	llc_store_misses.reset();
	cache_refs.enable();
	cache_misses.enable();
	llc_stores.enable();
	llc_store_misses.enable();

	// Benchmark
	std::vector<double> samples;
	{
		std::mutex mtx;
		cv::Mat lastImage;
		for (size_t i = 0; i < frames.size(); i++) {
			double us = process_frame(lastImage, mtx, frames[i]);
			samples.push_back(us);
		}
	}

	// Disable counters and read values
	cache_refs.disable();
	cache_misses.disable();
	llc_stores.disable();
	llc_store_misses.disable();

	long long cr = cache_refs.read_count();
	long long cm = cache_misses.read_count();
	long long ls = llc_stores.read_count();
	long long lm = llc_store_misses.read_count();

	std::sort(samples.begin(), samples.end());
	double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
	double mean = sum / samples.size();
	double median = samples[samples.size() / 2];
	double p95 = samples[(size_t)(samples.size() * 0.95)];

	printf("\n=== obs-backgroundremoval benchmark ===\n");
	printf("Frames: %zu @ %dx%d BGRA (%.1f MB/frame)\n", samples.size(), frames[0].cols, frames[0].rows,
	       frame_bytes / (1024.0 * 1024.0));
	printf("  mean:   %.1f us/frame\n", mean);
	printf("  median: %.1f us/frame\n", median);
	printf("  p95:    %.1f us/frame\n", p95);
	printf("  total:  %.1f ms (%zu frames)\n", sum / 1000.0, samples.size());
	if (has_perf) {
		printf("  cache-misses:     %lld (%.1f%% of %lld refs)\n", cm, cr > 0 ? 100.0 * cm / cr : 0.0, cr);
		printf("  LLC-store-misses: %lld (%.1f%% of %lld stores)\n", lm, ls > 0 ? 100.0 * lm / ls : 0.0, ls);
	}
	// CPU utilization test: real ORT thread pool with 30fps pacing
#if defined(HAS_ONNXRUNTIME) && !defined(_WIN32)
	if (!model_path.empty()) {
		const int cpu_frames = 90; // 3 seconds at 30fps
		const auto interval = std::chrono::microseconds(33333);
		const int ort_threads = 4;
		const bool ort_spinning = false;

		// Suppress OpenCV thread pool for this test so only ORT
		// threads contribute to CPU utilization.
		cv::setNumThreads(1);

		Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "bench");
		Ort::SessionOptions opts;
		opts.SetIntraOpNumThreads(ort_threads);
		opts.SetInterOpNumThreads(ort_threads);
		opts.AddConfigEntry("session.intra_op.allow_spinning", ort_spinning ? "1" : "0");
		opts.AddConfigEntry("session.inter_op.allow_spinning", ort_spinning ? "1" : "0");

		// Count process threads via /proc/self/status
		auto count_threads = []() -> int {
			FILE *f = fopen("/proc/self/status", "r");
			if (!f)
				return -1;
			char line[256];
			while (fgets(line, sizeof(line), f)) {
				int n;
				if (sscanf(line, "Threads: %d", &n) == 1) {
					fclose(f);
					return n;
				}
			}
			fclose(f);
			return -1;
		};

		int threads_before = count_threads();

		// Create session — thread pool is lazily initialized.
		Ort::Session session(env, model_path.c_str(), opts);

		// Set up inference inputs for the tiny Identity model.
		// Running inference each frame keeps ORT's thread pool
		// active — without periodic calls, spinning threads hit
		// a timeout and go to sleep, hiding the CPU cost.
		std::vector<float> input_data = {1.0f};
		std::vector<int64_t> input_shape = {1};
		auto mem_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
		auto input_tensor =
			Ort::Value::CreateTensor<float>(mem_info, input_data.data(), 1, input_shape.data(), 1);
		const char *input_names[] = {"x"};
		const char *output_names[] = {"y"};

		// Warmup inference to create thread pool
		session.Run(Ort::RunOptions{nullptr}, input_names, &input_tensor, 1, output_names, 1);

		int threads_after = count_threads();

		double cpu0 = get_cpu_seconds();
		auto wall0 = Clock::now();

		std::mutex mtx;
		cv::Mat lastImage;

		for (int i = 0; i < cpu_frames; i++) {
			auto t = Clock::now();
			process_frame(lastImage, mtx, frames[i % frames.size()]);
			// Run inference to keep ORT thread pool spinning
			session.Run(Ort::RunOptions{nullptr}, input_names, &input_tensor, 1, output_names, 1);
			std::this_thread::sleep_until(t + interval);
		}

		auto wall1 = Clock::now();
		double cpu1 = get_cpu_seconds();

		double wall_s = duration<double>(wall1 - wall0).count();
		double cpu_s = cpu1 - cpu0;

		printf("--- CPU utilization (30fps, %d frames, %d ORT threads) ---\n", cpu_frames, ort_threads);
		printf("  spinning: %s\n", ort_spinning ? "on" : "off");
		printf("  threads:  %d -> %d (ORT created %d)\n", threads_before, threads_after,
		       threads_after - threads_before);
		printf("  wall:     %.2f s\n", wall_s);
		printf("  cpu:      %.2f s (%.0f%%, %.1f cores)\n", cpu_s, 100.0 * cpu_s / wall_s, cpu_s / wall_s);
	}
#endif

	printf("===\n");

	return 0;
}
