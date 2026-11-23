// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ObsHarness.hpp"

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>
#include <utility>

#include <graphics/graphics.h>
#include <graphics/vec4.h>
#include <util/base.h>

#if defined(_WIN32) && !defined(OBS_TEST_ROOT)
#error OBS_TEST_ROOT must be defined by the build system
#endif
#ifndef OBS_TEST_MODULE_DIR
#error OBS_TEST_MODULE_DIR must be defined by the build system
#endif
#ifndef OBS_TEST_MODULE_DATA_DIR
#error OBS_TEST_MODULE_DATA_DIR must be defined by the build system
#endif
#ifndef OBS_TEST_MODULE_SUFFIX
#error OBS_TEST_MODULE_SUFFIX must be defined by the build system
#endif
#ifndef PLUGIN_FILE
#error PLUGIN_FILE must be defined by the build system
#endif
#ifndef PLUGIN_DATA_DIR
#error PLUGIN_DATA_DIR must be defined by the build system
#endif
#ifndef PLUGIN_VERSION_STR
#error PLUGIN_VERSION_STR must be defined by the build system
#endif
#ifndef TEST_DATA_DIR
#error TEST_DATA_DIR must be defined by the build system
#endif
#if defined(__APPLE__) && !defined(OBS_MACOS_FRAMEWORKS_DIR)
#error OBS_MACOS_FRAMEWORKS_DIR must be defined by the build system
#endif

namespace ObsHarness {

namespace {

std::mutex logMutex;
std::vector<std::string> logBuffer;

void logHandler(int level, const char *format, va_list args, void *)
{
	char line[8192];
	std::vsnprintf(line, sizeof(line), format, args);
	if (level <= LOG_WARNING) {
		std::cerr << line << '\n';
	}
	const std::lock_guard lock(logMutex);
	logBuffer.emplace_back(line);
}

std::filesystem::path configDirectory;

/// The binary of an OBS Studio module. On macOS every module is an application-bundle plugin, the
/// layout add_default_module_paths() of libobs looks for; elsewhere it is a shared module file.
auto moduleFile(const char *name) -> std::string
{
#ifdef __APPLE__
	return std::string(OBS_TEST_MODULE_DIR) + "/" + name + ".plugin/Contents/MacOS/" + name;
#else
	return std::string(OBS_TEST_MODULE_DIR) + "/" + name + OBS_TEST_MODULE_SUFFIX;
#endif
}

/// The data directory of an OBS Studio module, next to its binary in the same layout.
auto moduleDataPath(const char *name) -> std::string
{
#ifdef __APPLE__
	return std::string(OBS_TEST_MODULE_DATA_DIR) + "/" + name + ".plugin/Contents/Resources";
#else
	return std::string(OBS_TEST_MODULE_DATA_DIR) + "/" + name;
#endif
}

auto openModule(const std::string &file, const std::string &dataPath, std::string &error) -> bool
{
	obs_module_t *module = nullptr;
	const int result = obs_open_module(&module, file.c_str(), dataPath.c_str());
	if (result != MODULE_SUCCESS) {
		error = "obs_open_module failed for " + file + " (" + std::to_string(result) + ")";
		return false;
	}
	if (!obs_init_module(module)) {
		error = "obs_init_module failed for " + file;
		return false;
	}
	return true;
}

/// Records this version as already run with update checks off, so loading the plugin neither
/// schedules the first-run dialog nor reaches the network.
void writeUpdateConfig()
{
	const std::filesystem::path pluginConfig = configDirectory / "obs-backgroundremoval";
	std::filesystem::create_directories(pluginConfig);
	std::ofstream(pluginConfig / "update.ini")
		<< "[update]\nversion=" << PLUGIN_VERSION_STR << "\ncheck_for_updates=false\n";
}

auto createSource(const char *id, const char *name, const std::string &settingsJson) -> OBSSourceAutoRelease
{
	const OBSDataAutoRelease settings = obs_data_create_from_json(settingsJson.c_str());
	return obs_source_create(id, name, settings, nullptr);
}

/// On Linux, libobs needs an X display: says so when DISPLAY is unset.
void explainMissingDisplay([[maybe_unused]] std::string &error)
{
#if !defined(_WIN32) && !defined(__APPLE__)
	if (!std::getenv("DISPLAY")) {
		error += ": DISPLAY is unset; run the tests under xvfb-run";
	}
#endif
}

auto createImageSource(const std::string &imageFile, const char *name) -> OBSSourceAutoRelease
{
	const OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_string(settings, "file", imageFile.c_str());
	return obs_source_create("image_source", name, settings, nullptr);
}

} // namespace

auto start(std::string &error) -> bool
{
	base_set_log_handler(logHandler, nullptr);

	configDirectory = std::filesystem::temp_directory_path() /
			  ("obs-backgroundremoval-test-" +
			   std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	writeUpdateConfig();

	if (!obs_startup("en-US", configDirectory.string().c_str(), nullptr)) {
		error = "obs_startup failed";
		explainMissingDisplay(error);
		return false;
	}

	obs_video_info ovi{};
#ifdef _WIN32
	obs_add_data_path((std::string(OBS_TEST_ROOT) + "/data/libobs/").c_str());
	ovi.graphics_module = "libobs-d3d11";
#elif defined(__APPLE__)
	// libobs finds its own effects in its framework bundle. The backend is given by its full path:
	// os_dlopen() appends .so to a name without an extension.
	ovi.graphics_module = OBS_MACOS_FRAMEWORKS_DIR "/libobs-opengl.dylib";
#else
	// libobs finds its own effects on Linux, and renders through EGL on the X display in
	// DISPLAY.
	ovi.graphics_module = "libobs-opengl";
#endif
	ovi.fps_num = 30;
	ovi.fps_den = 1;
	ovi.base_width = 1280;
	ovi.base_height = 720;
	ovi.output_width = 1280;
	ovi.output_height = 720;
	ovi.output_format = VIDEO_FORMAT_NV12;
	ovi.adapter = 0;
	ovi.gpu_conversion = true;
	ovi.colorspace = VIDEO_CS_709;
	ovi.range = VIDEO_RANGE_PARTIAL;
	ovi.scale_type = OBS_SCALE_BICUBIC;
	if (const int result = obs_reset_video(&ovi); result != OBS_VIDEO_SUCCESS) {
		error = "obs_reset_video failed (" + std::to_string(result) + ")";
		explainMissingDisplay(error);
		return false;
	}

	for (const char *name : {"image-source", "obs-filters"}) {
		if (!openModule(moduleFile(name), moduleDataPath(name), error)) {
			return false;
		}
	}
	return openModule(PLUGIN_FILE, PLUGIN_DATA_DIR, error);
}

void stop()
{
	// Also called after start() failed, when obs_startup may have failed and libobs is not running.
	if (obs_initialized()) {
		obs_set_output_source(0, nullptr);
		obs_shutdown();
	}
	std::error_code ignored;
	std::filesystem::remove_all(configDirectory, ignored);
}

auto logLines() -> std::vector<std::string>
{
	const std::lock_guard lock(logMutex);
	return logBuffer;
}

void clearLog()
{
	const std::lock_guard lock(logMutex);
	logBuffer.clear();
}

auto countLogLines(const std::string &line) -> std::size_t
{
	const std::lock_guard lock(logMutex);
	return static_cast<std::size_t>(std::count(logBuffer.begin(), logBuffer.end(), line));
}

void waitFrames(int frames)
{
	using namespace std::chrono_literals;
	const auto deadline = std::chrono::steady_clock::now() + 30s;
	std::uint64_t last = obs_get_video_frame_time();
	for (int seen = 0; seen < frames && std::chrono::steady_clock::now() < deadline;) {
		std::this_thread::sleep_for(1ms);
		const std::uint64_t now = obs_get_video_frame_time();
		if (now != last) {
			last = now;
			++seen;
		}
	}
}

auto testDataFile(const char *name) -> std::string
{
	return std::string(TEST_DATA_DIR) + "/" + name;
}

auto capture(obs_source_t *source, std::uint32_t width, std::uint32_t height) -> Image
{
	Image image{width, height, std::vector<std::uint8_t>(static_cast<std::size_t>(width) * height * 4)};
	if (width == 0 || height == 0) {
		return image;
	}

	obs_enter_graphics();
	gs_texrender_t *texrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	if (gs_texrender_begin(texrender, width, height)) {
		vec4 clearColor;
		vec4_zero(&clearColor);
		gs_clear(GS_CLEAR_COLOR, &clearColor, 0.0f, 0);
		gs_ortho(0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height), -100.0f, 100.0f);
		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
		obs_source_video_render(source);
		gs_blend_state_pop();
		gs_texrender_end(texrender);

		gs_stagesurf_t *stage = gs_stagesurface_create(width, height, GS_RGBA);
		gs_stage_texture(stage, gs_texrender_get_texture(texrender));
		std::uint8_t *data = nullptr;
		std::uint32_t linesize = 0;
		if (gs_stagesurface_map(stage, &data, &linesize)) {
			for (std::uint32_t y = 0; y < height; ++y) {
				std::copy_n(data + static_cast<std::size_t>(y) * linesize, width * 4,
					    image.rgba.begin() + static_cast<std::ptrdiff_t>(y) * width * 4);
			}
			gs_stagesurface_unmap(stage);
		}
		gs_stagesurface_destroy(stage);
	}
	gs_texrender_destroy(texrender);
	obs_leave_graphics();
	return image;
}

auto capture(obs_source_t *source) -> Image
{
	return capture(source, obs_source_get_width(source), obs_source_get_height(source));
}

auto foregroundMask(const Image &image) -> std::vector<bool>
{
	std::vector<bool> mask(image.pixelCount());
	for (std::size_t i = 0; i < mask.size(); ++i) {
		mask[i] = image.rgba[i * 4 + 3] >= 128;
	}
	return mask;
}

auto transparentShare(const Image &image) -> double
{
	std::size_t count = 0;
	for (std::size_t i = 0; i < image.pixelCount(); ++i) {
		count += image.rgba[i * 4 + 3] < 128 ? 1 : 0;
	}
	return image.pixelCount() ? static_cast<double>(count) / static_cast<double>(image.pixelCount()) : 0.0;
}

auto opaqueShare(const Image &image) -> double
{
	std::size_t count = 0;
	for (std::size_t i = 0; i < image.pixelCount(); ++i) {
		count += image.rgba[i * 4 + 3] == 255 ? 1 : 0;
	}
	return image.pixelCount() ? static_cast<double>(count) / static_cast<double>(image.pixelCount()) : 0.0;
}

auto meanChange(const Image &a, const Image &b, const std::vector<bool> &mask) -> double
{
	double sum = 0.0;
	std::size_t count = 0;
	for (std::size_t i = 0; i < a.pixelCount() && i < b.pixelCount() && i < mask.size(); ++i) {
		if (!mask[i]) {
			continue;
		}
		for (std::size_t c = 0; c < 3; ++c) {
			sum += std::abs(static_cast<int>(a.rgba[i * 4 + c]) - static_cast<int>(b.rgba[i * 4 + c]));
		}
		++count;
	}
	return count ? sum / (3.0 * static_cast<double>(count)) : 0.0;
}

auto meanChange(const Image &a, const Image &b) -> double
{
	return meanChange(a, b, std::vector<bool>(a.pixelCount(), true));
}

auto maxDifference(const Image &a, const Image &b) -> int
{
	if (a.rgba.size() != b.rgba.size()) {
		return 256;
	}
	int result = 0;
	for (std::size_t i = 0; i < a.rgba.size(); ++i) {
		result = std::max(result, std::abs(static_cast<int>(a.rgba[i]) - static_cast<int>(b.rgba[i])));
	}
	return result;
}

auto backgroundIsRemoved(const Image &image) -> bool
{
	const double share = transparentShare(image);
	return share > 0.1 && share < 0.9;
}

Scene::Scene(const std::string &imageFile, std::vector<FilterSpec> filters, Visibility visibility)
	: visibility_(visibility)
{
	source_ = createImageSource(imageFile, "Portrait");
	clearLog();
	int index = 0;
	for (const FilterSpec &spec : filters) {
		const std::string name = spec.id + " " + std::to_string(index++);
		OBSSourceAutoRelease filter = createSource(spec.id.c_str(), name.c_str(), spec.settingsJson);
		obs_source_filter_add(source_, filter);
		filters_.push_back(std::move(filter));
	}
	scene_ = obs_scene_create("Scene");
	obs_scene_add(scene_, source_);
	if (visibility_ == Visibility::Program) {
		obs_set_output_source(0, obs_scene_get_source(scene_));
	} else if (visibility_ == Visibility::ShowingOnly) {
		obs_source_inc_showing(obs_scene_get_source(scene_));
	}
	waitFrames(10);
}

Scene::~Scene()
{
	if (visibility_ == Visibility::Program) {
		obs_set_output_source(0, nullptr);
	} else if (visibility_ == Visibility::ShowingOnly) {
		obs_source_dec_showing(obs_scene_get_source(scene_));
	}
	waitFrames(2);
	for (const OBSSourceAutoRelease &filter : filters_) {
		obs_source_filter_remove(source_, filter);
	}
	filters_.clear();
	// Since OBS Studio 31, the main canvas keeps every scene obs_scene_create() made until the
	// scene is removed, as OBS Studio does when a scene is deleted.
	obs_source_remove(obs_scene_get_source(scene_));
	scene_ = nullptr;
	source_ = nullptr;
	extraSources_.clear();
	waitFrames(2);
}

auto Scene::backgroundRemoval() const -> obs_source_t *
{
	for (const OBSSourceAutoRelease &filter : filters_) {
		if (std::string(obs_source_get_id(filter)) == kBackgroundRemovalId) {
			return filter;
		}
	}
	return nullptr;
}

void Scene::update(obs_source_t *filter, const std::string &settingsJson)
{
	const OBSDataAutoRelease settings = obs_data_create_from_json(settingsJson.c_str());
	obs_source_update(filter, settings);
	waitFrames(10);
}

void Scene::setEnabled(obs_source_t *filter, bool enabled)
{
	obs_source_set_enabled(filter, enabled);
	waitFrames(10);
}

void Scene::addSecondSource(const std::string &imageFile, const std::vector<FilterSpec> &filters)
{
	OBSSourceAutoRelease source = createImageSource(imageFile, "Second portrait");
	int index = 0;
	for (const FilterSpec &spec : filters) {
		const std::string name = "second " + spec.id + " " + std::to_string(index++);
		const OBSSourceAutoRelease filter = createSource(spec.id.c_str(), name.c_str(), spec.settingsJson);
		obs_source_filter_add(source, filter);
	}
	obs_scene_add(scene_, source);
	extraSources_.push_back(std::move(source));
}

auto captureUnfiltered(const std::string &imageFile) -> Image
{
	const Scene scene(imageFile, {});
	return scene.capture();
}

auto pipelineCreatedLine(std::uint32_t width, std::uint32_t height) -> std::string
{
	return "[obs-backgroundremoval] Creating the MediaPipe (Landscape) shader pipeline (" + std::to_string(width) +
	       "x" + std::to_string(height) + ")";
}

auto anyPipelineCreated() -> bool
{
	const std::string prefix = "[obs-backgroundremoval] Creating the MediaPipe (Landscape) shader pipeline";
	for (const std::string &line : logLines()) {
		if (line.rfind(prefix, 0) == 0) {
			return true;
		}
	}
	return false;
}

} // namespace ObsHarness
