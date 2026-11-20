// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

// libobs, started headless from an OBS Studio release with the plugin and the OBS Studio filters
// loaded, and the scenes the cases of SP-01006 to SP-01019 build on it.

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <obs.h>

namespace ObsHarness {

/// Starts libobs and loads the modules; returns false with a message on failure.
auto start(std::string &error) -> bool;
void stop();

/// Every line libobs and the modules logged since the last clearLog().
auto logLines() -> std::vector<std::string>;
void clearLog();
auto countLogLines(const std::string &line) -> std::size_t;

/// Waits until the video thread of libobs has rendered `frames` more frames.
void waitFrames(int frames);

/// The full path of a file in tests/data.
auto testDataFile(const char *name) -> std::string;

/// An RGBA image read back from the GPU.
struct Image {
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	std::vector<std::uint8_t> rgba;

	[[nodiscard]] auto alpha(std::uint32_t x, std::uint32_t y) const -> std::uint8_t
	{
		return rgba[(static_cast<std::size_t>(y) * width + x) * 4 + 3];
	}
	[[nodiscard]] auto pixelCount() const -> std::size_t { return static_cast<std::size_t>(width) * height; }
};

/// Renders the source with its filters into a width x height RGBA target and reads it back.
auto capture(obs_source_t *source, std::uint32_t width, std::uint32_t height) -> Image;
/// Renders the source at its own size.
auto capture(obs_source_t *source) -> Image;

/// Pixels with alpha of at least 128.
auto foregroundMask(const Image &image) -> std::vector<bool>;
/// Share of pixels with alpha below 128.
auto transparentShare(const Image &image) -> double;
/// Share of pixels with alpha 255.
auto opaqueShare(const Image &image) -> double;
/// Mean absolute difference of the color channels over the pixels selected by the mask, 0 to 255.
auto meanChange(const Image &a, const Image &b, const std::vector<bool> &mask) -> double;
/// Mean absolute difference of the color channels over all pixels.
auto meanChange(const Image &a, const Image &b) -> double;
/// The largest difference of any channel, alpha included, over all pixels.
auto maxDifference(const Image &a, const Image &b) -> int;

/// Whether a foreground is kept and a background is cut away, as Background Removal does.
auto backgroundIsRemoved(const Image &image) -> bool;

enum class Visibility {
	Program,     ///< In the program output: active and showing.
	ShowingOnly, ///< Showing but not active, as in a preview.
	Hidden,      ///< Neither active nor showing.
};

/// A filter added to the image source, by type and settings as JSON.
struct FilterSpec {
	std::string id;
	std::string settingsJson = "{}";
};

/// An image source with a chain of filters, shown as requested. The log is cleared before the
/// filters are added, so it holds everything they log from their creation on.
class Scene final {
public:
	Scene(const std::string &imageFile, std::vector<FilterSpec> filters,
	      Visibility visibility = Visibility::Program);
	~Scene();
	Scene(const Scene &) = delete;
	auto operator=(const Scene &) -> Scene & = delete;

	[[nodiscard]] auto source() const -> obs_source_t * { return source_; }
	[[nodiscard]] auto filter(std::size_t index) const -> obs_source_t * { return filters_.at(index); }
	/// The Background Removal filter, the first in the chain of that type.
	[[nodiscard]] auto backgroundRemoval() const -> obs_source_t *;

	[[nodiscard]] auto width() const -> std::uint32_t { return obs_source_get_width(source_); }
	[[nodiscard]] auto height() const -> std::uint32_t { return obs_source_get_height(source_); }

	/// Merges the JSON settings into the filter and waits for the video thread to apply them.
	void update(obs_source_t *filter, const std::string &settingsJson);
	void setEnabled(obs_source_t *filter, bool enabled);

	/// Renders the source with its filters at its own size.
	[[nodiscard]] auto capture() const -> Image { return ObsHarness::capture(source_); }

	/// Adds another image source with the same filter chain to this scene.
	void addSecondSource(const std::string &imageFile, const std::vector<FilterSpec> &filters);

private:
	obs_source_t *source_ = nullptr;
	std::vector<obs_source_t *> filters_;
	obs_scene_t *scene_ = nullptr;
	Visibility visibility_;
	std::vector<obs_source_t *> extraSources_;
};

/// The image source alone, without filters, rendered at its own size.
auto captureUnfiltered(const std::string &imageFile) -> Image;

inline constexpr const char *kBackgroundRemovalId = "background_removal";

/// The line the plugin logs when it creates its shader pipeline.
auto pipelineCreatedLine(std::uint32_t width, std::uint32_t height, int factor) -> std::string;
/// Whether the log holds any pipeline creation.
auto anyPipelineCreated() -> bool;

} // namespace ObsHarness
