// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <objbase.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <TraceLoggingProvider.h>

#include <benchmark/benchmark.h>

#include <obs.h>
#include <graphics/graphics.h>

#include <BackgroundRemoval/windows/FilterProperty.hpp>
#include <BackgroundRemoval/windows/MPLD10Pipeline.hpp>

TRACELOGGING_DECLARE_PROVIDER(g_hProvider);

namespace MPLD10CameraBenchmark {
struct Sample {
	std::uint64_t frame;
	double tickMs, renderMs;
	std::uint64_t callbackBeginQpc, callbackEndQpc;
};
struct State {
	obs_source_t *source;
	std::unique_ptr<BackgroundRemoval::MPLD10Pipeline> pipeline;
	std::atomic<bool> collecting{false};
	std::atomic<unsigned> failures{0};
	std::atomic<unsigned> ticks{0}, renders{0};
	std::atomic<unsigned> width{0}, height{0};
	std::vector<Sample> samples;
	double tickMs = 0;
	double sensitivity = 60;
};
auto elapsed(std::chrono::steady_clock::time_point start) -> double
{
	return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}
auto name(void *) -> const char *
{
	return "MPLD10 camera diagnostic filter";
}
auto create(obs_data_t *settings, obs_source_t *source) -> void *
{
	auto state = std::make_unique<State>();
	state->source = source;
	state->sensitivity = obs_data_get_double(settings, "sensitivity");
	state->samples.reserve(4000);
	return state.release();
}
void destroy(void *data)
{
	delete static_cast<State *>(data);
}
void tick(void *data, float seconds)
{
	auto &state = *static_cast<State *>(data);
	state.ticks.fetch_add(1, std::memory_order_relaxed);
	const auto start = std::chrono::steady_clock::now();
	auto *target = obs_filter_get_target(state.source);
	const auto width = target ? obs_source_get_base_width(target) : 0;
	const auto height = target ? obs_source_get_base_height(target) : 0;
	state.width.store(width, std::memory_order_relaxed);
	state.height.store(height, std::memory_order_relaxed);
	if (width && height &&
	    (!state.pipeline || state.pipeline->needsRecreation() || state.pipeline->getWidth() != width ||
	     state.pipeline->getHeight() != height)) {
		obs_enter_graphics();
		state.pipeline.reset();
		try {
			BackgroundRemoval::FilterProperty property{};
			property.pipelineId = BackgroundRemoval::MPLD10Pipeline::kPipelineId;
			property.imageSimilarityThreshold = state.sensitivity;
			property.refineMaskThreshold = .5;
			state.pipeline = std::make_unique<BackgroundRemoval::MPLD10Pipeline>(property, state.source,
											     width, height);
		} catch (const std::exception &e) {
			blog(LOG_ERROR, "Camera benchmark initialization: %s", e.what());
			state.failures.fetch_add(1, std::memory_order_relaxed);
		}
		obs_leave_graphics();
	}
	if (state.pipeline)
		state.pipeline->videoTick(seconds);
	state.tickMs = elapsed(start);
}
void render(void *data, gs_effect_t *effect)
{
	auto &state = *static_cast<State *>(data);
	if (!state.pipeline) {
		obs_source_skip_video_filter(state.source);
		return;
	}
	LARGE_INTEGER callbackBegin{}, callbackEnd{};
	QueryPerformanceCounter(&callbackBegin);
	const auto start = std::chrono::steady_clock::now();
	state.pipeline->videoRender(effect);
	state.renders.fetch_add(1, std::memory_order_release);
	QueryPerformanceCounter(&callbackEnd);
	if (state.collecting.load(std::memory_order_acquire))
		state.samples.push_back({obs_get_video_frame_time(), state.tickMs, elapsed(start),
					 static_cast<std::uint64_t>(callbackBegin.QuadPart),
					 static_cast<std::uint64_t>(callbackEnd.QuadPart)});
}
auto colorSpace(void *, size_t, const gs_color_space *) -> gs_color_space
{
	return GS_CS_709_EXTENDED;
}
void synchronize(void *) {}
struct Pattern {
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> image;
	std::unique_ptr<gs_texture_t, void (*)(gs_texture_t *)> alternate{nullptr, gs_texture_destroy};
	unsigned frame = 0;
	explicit Pattern(gs_texture_t *texture) : image(texture, gs_texture_destroy) {}
	~Pattern()
	{
		obs_enter_graphics();
		image.reset();
		alternate.reset();
		obs_leave_graphics();
	}
};
auto patternCreate(obs_data_t *settings, obs_source_t *) -> void *
{
	std::vector<std::uint8_t> pixels(1280 * 720 * 4, 255);
	for (unsigned y = 0; y < 720; ++y)
		for (unsigned x = 0; x < 1280; ++x) {
			auto *pixel = pixels.data() + (y * 1280 + x) * 4;
			pixel[0] = static_cast<std::uint8_t>(x / 5 + y / 3);
			pixel[1] = static_cast<std::uint8_t>(y / 3);
			pixel[2] = static_cast<std::uint8_t>(x / 5);
		}
	const auto *bytes = pixels.data();
	obs_enter_graphics();
	auto *texture = gs_texture_create(1280, 720, GS_BGRA, 1, &bytes, 0);
	obs_leave_graphics();
	if (!texture)
		return nullptr;
	auto pattern = std::make_unique<Pattern>(texture);
	if (obs_data_get_bool(settings, "animated")) {
		for (size_t i = 0; i < pixels.size(); i += 4)
			for (size_t channel = 0; channel < 3; ++channel)
				pixels[i + channel] = 255 - pixels[i + channel];
		obs_enter_graphics();
		pattern->alternate.reset(gs_texture_create(1280, 720, GS_BGRA, 1, &bytes, 0));
		obs_leave_graphics();
		if (!pattern->alternate)
			return nullptr;
	}
	return pattern.release();
}
void patternTick(void *data, float)
{
	++static_cast<Pattern *>(data)->frame;
}
void patternDestroy(void *data)
{
	delete static_cast<Pattern *>(data);
}
void patternRender(void *data, gs_effect_t *)
{
	auto &pattern = *static_cast<Pattern *>(data);
	obs_source_draw(pattern.alternate && (pattern.frame & 1) ? pattern.alternate.get() : pattern.image.get(), 0, 0,
			1280, 720, false);
}
auto patternWidth(void *) -> uint32_t
{
	return 1280;
}
auto patternHeight(void *) -> uint32_t
{
	return 720;
}
void previewDraw(void *, uint32_t cx, uint32_t cy)
{
	gs_set_viewport(0, 0, cx, cy);
	gs_ortho(0, 1280, 0, 720, -100, 100);
	obs_render_main_texture();
}
void pause(double seconds, HWND preview)
{
	if (!preview) {
		Sleep(static_cast<DWORD>(seconds * 1000));
		return;
	}
	const auto start = std::chrono::steady_clock::now();
	while (elapsed(start) < seconds * 1000) {
		MSG message;
		while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&message);
			DispatchMessageW(&message);
		}
		MsgWaitForMultipleObjectsEx(0, nullptr, 10, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
	}
}

int run(int argc, char **argv)
{
	std::filesystem::path settingsPath, output;
	double seconds = 15, sensitivity = 60;
	bool preview = false;
	bool synthetic = false;
	bool animated = false;
	unsigned fps = 30;
	// Some PIX versions reject --command-line. CLI options override these environment defaults.
	if (const auto *value = std::getenv("MPLD10_BENCHMARK_OUTPUT"))
		output = std::filesystem::u8path(value);
	if (const auto *value = std::getenv("MPLD10_BENCHMARK_SECONDS"))
		seconds = std::stod(value);
	if (const auto *value = std::getenv("MPLD10_BENCHMARK_INPUT")) {
		synthetic = std::string(value) == "static" || std::string(value) == "animated";
		animated = std::string(value) == "animated";
	}
	for (int i = 1; i < argc; ++i) {
		const std::string option = argv[i];
		if (option == "--animated") {
			synthetic = animated = true;
			continue;
		}
		if (option == "--synthetic") {
			synthetic = true;
			animated = false;
			continue;
		}
		if (option == "--preview") {
			preview = true;
			continue;
		}
		if (option == "--help") {
			std::cout
				<< "--camera-settings FILE --output DIRECTORY [--seconds 15] [--fps 30] [--sensitivity 60] [--preview] [--synthetic | --animated]\n";
			return 0;
		}
		if (++i == argc)
			throw std::invalid_argument("Missing option value");
		if (option == "--camera-settings")
			settingsPath = std::filesystem::u8path(argv[i]);
		else if (option == "--output")
			output = std::filesystem::u8path(argv[i]);
		else if (option == "--seconds")
			seconds = std::stod(argv[i]);
		else if (option == "--sensitivity")
			sensitivity = std::stod(argv[i]);
		else if (option == "--fps")
			fps = std::stoul(argv[i]);
		else
			throw std::invalid_argument("Unknown option");
	}
	if ((!synthetic && settingsPath.empty()) || output.empty() || !fps || fps > 120 || !std::isfinite(seconds) ||
	    seconds < 1 || seconds > 90 || !std::isfinite(sensitivity) || sensitivity < 0 || sensitivity > 100)
		throw std::invalid_argument("Invalid camera benchmark arguments");
	if (std::filesystem::exists(output / "samples.csv"))
		throw std::invalid_argument("Output already exists");
	std::filesystem::create_directories(output);
	if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
		throw std::runtime_error("COM initialization failed");
	struct ComCleanup {
		~ComCleanup() { CoUninitialize(); }
	} com;
	if (!obs_startup("en-US", nullptr, nullptr))
		throw std::runtime_error("OBS startup failed");
	struct ObsCleanup {
		~ObsCleanup() { obs_shutdown(); }
	} obs;
	obs_add_data_path(OBS_TEST_BIN_DIR "/../../data/libobs/");
	obs_audio_info audio{};
	audio.samples_per_sec = 48000;
	audio.speakers = SPEAKERS_STEREO;
	if (!obs_reset_audio(&audio))
		throw std::runtime_error("OBS audio initialization failed");
	obs_video_info video{};
	video.graphics_module = OBS_TEST_BIN_DIR "/libobs-d3d11.dll";
	video.fps_num = fps;
	video.fps_den = 1;
	video.base_width = video.output_width = 1280;
	video.base_height = video.output_height = 720;
	video.output_format = VIDEO_FORMAT_NV12;
	video.colorspace = VIDEO_CS_709;
	video.range = VIDEO_RANGE_PARTIAL;
	video.gpu_conversion = true;
	if (obs_reset_video(&video) != OBS_VIDEO_SUCCESS)
		throw std::runtime_error("OBS video initialization failed");
	obs_module_t *module = nullptr;
	if (!synthetic && (obs_open_module(&module, OBS_TEST_BIN_DIR "/../../obs-plugins/64bit/win-dshow.dll",
					   OBS_TEST_BIN_DIR "/../../data/obs-plugins/win-dshow") != MODULE_SUCCESS ||
			   !obs_init_module(module)))
		throw std::runtime_error("Camera module initialization failed");
	if (synthetic) {
		obs_source_info pattern{};
		pattern.id = "mpld10-diagnostic-pattern";
		pattern.type = OBS_SOURCE_TYPE_INPUT;
		pattern.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW;
		pattern.get_name = name;
		pattern.create = patternCreate;
		pattern.destroy = patternDestroy;
		pattern.video_render = patternRender;
		pattern.video_tick = patternTick;
		pattern.get_width = patternWidth;
		pattern.get_height = patternHeight;
		obs_register_source(&pattern);
	}
	obs_source_info info{};
	info.id = "mpld10-camera-diagnostic";
	info.type = OBS_SOURCE_TYPE_FILTER;
	info.output_flags = OBS_SOURCE_VIDEO;
	info.get_name = name;
	info.create = create;
	info.destroy = destroy;
	info.video_tick = tick;
	info.video_render = render;
	info.video_get_color_space = colorSpace;
	obs_register_source(&info);
	std::unique_ptr<obs_data_t, void (*)(obs_data_t *)> settings{
		synthetic ? obs_data_create()
			  : obs_data_create_from_json_file(
				    reinterpret_cast<const char *>(settingsPath.u8string().c_str())),
		obs_data_release};
	if (!settings)
		throw std::runtime_error("Camera settings could not be read");
	if (synthetic)
		obs_data_set_bool(settings.get(), "animated", animated);
	// Accept an existing OBS scene collection read-only; pass only the camera's settings to dshow.
	std::unique_ptr<obs_data_array_t, void (*)(obs_data_array_t *)> sources{
		obs_data_get_array(settings.get(), "sources"), obs_data_array_release};
	if (sources) {
		bool cameraFound = false;
		for (size_t i = 0; i < obs_data_array_count(sources.get()); ++i) {
			std::unique_ptr<obs_data_t, void (*)(obs_data_t *)> item{obs_data_array_item(sources.get(), i),
										 obs_data_release};
			if (std::string(obs_data_get_string(item.get(), "id")) == "dshow_input") {
				if (cameraFound)
					throw std::runtime_error("Multiple cameras: supply one camera settings JSON");
				settings.reset(obs_data_get_obj(item.get(), "settings"));
				cameraFound = true;
			}
		}
		if (!cameraFound || !settings)
			throw std::runtime_error("No camera in scene collection");
	}
	std::unique_ptr<obs_source_t, void (*)(obs_source_t *)> camera{
		obs_source_create_private(synthetic ? "mpld10-diagnostic-pattern" : "dshow_input", "Diagnostic input",
					  settings.get()),
		obs_source_release};
	std::unique_ptr<obs_data_t, void (*)(obs_data_t *)> filterSettings{obs_data_create(), obs_data_release};
	obs_data_set_double(filterSettings.get(), "sensitivity", sensitivity);
	std::unique_ptr<obs_source_t, void (*)(obs_source_t *)> filter{
		obs_source_create_private(info.id, "Diagnostic MPLD10", filterSettings.get()), obs_source_release};
	if (!camera || !filter)
		throw std::runtime_error("Camera or filter creation failed");
	auto *state = static_cast<State *>(obs_obj_get_data(filter.get()));
	obs_source_set_audio_mixers(camera.get(), 0);
	obs_source_filter_add(camera.get(), filter.get());
	obs_set_output_source(0, camera.get());
	obs_source_inc_showing(camera.get());
	struct Detach {
		obs_source_t *camera;
		~Detach()
		{
			obs_set_output_source(0, nullptr);
			obs_source_dec_showing(camera);
			obs_queue_task(OBS_TASK_GRAPHICS, synchronize, nullptr, true);
		}
	} detach{camera.get()};
	WNDCLASSW windowClass{};
	windowClass.lpfnWndProc = DefWindowProcW;
	windowClass.hInstance = GetModuleHandleW(nullptr);
	windowClass.lpszClassName = L"MPLD10CameraDiagnostic";
	if (preview && !RegisterClassW(&windowClass))
		throw std::runtime_error("Preview window class failed");
	std::unique_ptr<std::remove_pointer_t<HWND>, decltype(&DestroyWindow)> window{
		preview ? CreateWindowExW(WS_EX_NOACTIVATE, windowClass.lpszClassName, L"MPLD10 camera diagnostic",
					  WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720, nullptr,
					  nullptr, windowClass.hInstance, nullptr)
			: nullptr,
		DestroyWindow};
	gs_init_data displayInfo{};
	displayInfo.cx = 1280;
	displayInfo.cy = 720;
	displayInfo.format = GS_BGRA;
	displayInfo.zsformat = GS_ZS_NONE;
	displayInfo.window.hwnd = window.get();
	std::unique_ptr<obs_display_t, void (*)(obs_display_t *)> display{
		preview && window ? obs_display_create(&displayInfo, 0) : nullptr, obs_display_destroy};
	if (preview) {
		if (!display)
			throw std::runtime_error("Preview creation failed");
		obs_display_add_draw_callback(display.get(), previewDraw, nullptr);
		ShowWindow(window.get(), SW_SHOWNOACTIVATE);
	}
	// Real libobs graphics thread drives tick/render. Main thread only sleeps.
	const auto startup = std::chrono::steady_clock::now();
	while (!state->renders.load(std::memory_order_acquire)) {
		if (state->failures.load() || elapsed(startup) >= 60000)
			throw std::runtime_error("Pipeline did not render a frame during startup");
		pause(.05, window.get());
	}
	pause(3, window.get());
	state->collecting.store(true, std::memory_order_release);
	pause(seconds, window.get());
	state->collecting.store(false, std::memory_order_release);
	obs_queue_task(OBS_TASK_GRAPHICS, synchronize, nullptr, true);
	std::cout << "CAMERA_CALLBACKS,ticks=" << state->ticks.load() << ",renders=" << state->renders.load()
		  << ",width=" << state->width.load() << ",height=" << state->height.load() << '\n';
	std::ofstream csv(output / "samples.csv");
	csv.exceptions(std::ios::badbit | std::ios::failbit);
	csv << "frame_ns,tick_ms,render_ms,callback_begin_qpc,callback_end_qpc\n" << std::setprecision(10);
	std::vector<double> durations;
	for (const auto &sample : state->samples) {
		csv << sample.frame << ',' << sample.tickMs << ',' << sample.renderMs << ',' << sample.callbackBeginQpc
		    << ',' << sample.callbackEndQpc << '\n';
		durations.push_back(sample.renderMs);
	}
	if (durations.empty() || !state->width.load() || state->failures.load())
		throw std::runtime_error("No valid camera rendering samples");
	std::sort(durations.begin(), durations.end());
	std::ofstream metadata(output / "conditions.txt");
	metadata << "source=" << (synthetic ? (animated ? "alternating-pattern" : "static-pattern") : "dshow_input")
		 << "\nreal_obs_graphics_thread=1\ncanvas=1280x720\nfps=" << fps << "\npreview=" << preview
		 << "\nprocess_id=" << GetCurrentProcessId() << "\ncamera_width=" << state->width.load()
		 << "\ncamera_height=" << state->height.load() << "\nsensitivity=" << sensitivity
		 << "\nrefine_mandatory=1\nrefine_threshold=0.5\nblur_enabled=0\n";
	// Report measured OBS callback samples; this is callback latency, not GPU time.
	std::string program = "MPLD10CameraBenchmark";
	std::string outputArgument = "--benchmark_out=" + (output / "benchmark.json").string();
	std::array<char *, 3> benchmarkArguments{program.data(), outputArgument.data(), nullptr};
	int benchmarkArgumentCount = 2;
	benchmark::Initialize(&benchmarkArgumentCount, benchmarkArguments.data());
	benchmark::RegisterBenchmark(
		"Camera/RenderCallback",
		[&](benchmark::State &measurement) {
			std::size_t sample = 0;
			for (auto _ : measurement)
				measurement.SetIterationTime(durations.at(sample++) / 1000.0);
			measurement.counters["median_ms"] = durations[durations.size() / 2];
			measurement.counters["p95_ms"] =
				durations[static_cast<size_t>(std::ceil(durations.size() * .95)) - 1];
		})
		->UseManualTime()
		->Iterations(static_cast<int64_t>(durations.size()))
		->Unit(benchmark::kMillisecond);
	benchmark::RunSpecifiedBenchmarks();
	benchmark::Shutdown();
	return 0;
}
} // namespace MPLD10CameraBenchmark

int wmain(int argc, wchar_t **wideArgv)
{
	if (TraceLoggingRegister(g_hProvider) != ERROR_SUCCESS)
		return 1;
	int result = 1;
	try {
		std::vector<std::string> arguments;
		std::vector<char *> argv;
		arguments.reserve(argc);
		for (int i = 0; i < argc; ++i) {
			const int size = WideCharToMultiByte(CP_UTF8, 0, wideArgv[i], -1, nullptr, 0, nullptr, nullptr);
			if (!size)
				throw std::runtime_error("Command line conversion failed");
			std::string value(size, '\0');
			WideCharToMultiByte(CP_UTF8, 0, wideArgv[i], -1, value.data(), size, nullptr, nullptr);
			value.pop_back();
			arguments.push_back(std::move(value));
		}
		for (auto &value : arguments)
			argv.push_back(value.data());
		result = MPLD10CameraBenchmark::run(argc, argv.data());
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
	}
	TraceLoggingUnregister(g_hProvider);
	return result;
}
