// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

// SP-01006 to SP-01015: the size, the pipeline lifecycle, and the visibility of the filter,
// running in libobs.

#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "ObsHarness.hpp"

using namespace ObsHarness;

namespace {

const std::string kPortrait = testDataFile("portrait-umireon.jpg");
const std::string kOddPortrait = testDataFile("portrait-umireon-1281x721.jpg");

auto backgroundRemoval(const std::string &settingsJson = "{}") -> FilterSpec
{
	return {kBackgroundRemovalId, settingsJson};
}

auto outputDownscaleFactor(int factor) -> std::string
{
	return R"({"v2_OutputDownscaleFactor": )" + std::to_string(factor) + "}";
}

struct SizeExample {
	int factor;
	std::uint32_t width;
	std::uint32_t height;
};

/// The share of pixels of the scaled mask that agree with the full-size mask sampled at their
/// centers.
auto maskAgreement(const Image &full, const Image &scaled, int factor) -> double
{
	const std::vector<bool> fullMask = foregroundMask(full);
	const std::vector<bool> scaledMask = foregroundMask(scaled);
	std::size_t agree = 0;
	for (std::uint32_t y = 0; y < scaled.height; ++y) {
		for (std::uint32_t x = 0; x < scaled.width; ++x) {
			const std::uint32_t fx = std::min(full.width - 1, x * factor + factor / 2);
			const std::uint32_t fy = std::min(full.height - 1, y * factor + factor / 2);
			agree += fullMask[fy * full.width + fx] == scaledMask[y * scaled.width + x] ? 1 : 0;
		}
	}
	return static_cast<double>(agree) / static_cast<double>(scaled.pixelCount());
}

} // namespace

// MARK: SP-01006 Output downscale factor

TEST_CASE("SP-01006.1 The output size follows the output downscale factor")
{
	Image full;
	{
		const Scene scene(kPortrait, {backgroundRemoval()});
		full = scene.capture();
	}
	REQUIRE(full.width == 1280);

	for (const SizeExample example : {SizeExample{1, 1280, 720}, {2, 640, 360}, {4, 320, 180}}) {
		CAPTURE(example.factor);
		Scene scene(kPortrait, {backgroundRemoval()});
		scene.update(scene.backgroundRemoval(), outputDownscaleFactor(example.factor));
		CHECK(scene.width() == example.width);
		CHECK(scene.height() == example.height);
		const Image scaled = scene.capture();
		CHECK(maskAgreement(full, scaled, example.factor) >= 0.99);
		CHECK(countLogLines(pipelineCreatedLine(1280, 720, example.factor)) >= 1);
	}
}

TEST_CASE("SP-01006.2 Returning to downscale factor 1 recreates the pipeline")
{
	Scene scene(kPortrait, {backgroundRemoval(outputDownscaleFactor(4))});
	clearLog();
	scene.update(scene.backgroundRemoval(), outputDownscaleFactor(1));
	CHECK(scene.width() == 1280);
	CHECK(scene.height() == 720);
	CHECK(countLogLines(pipelineCreatedLine(1280, 720, 1)) == 1);
}

// MARK: SP-01007 Odd source size

TEST_CASE("SP-01007.1 The output size rounds up")
{
	for (const SizeExample example : {SizeExample{1, 1281, 721}, {2, 641, 361}, {4, 321, 181}}) {
		CAPTURE(example.factor);
		Scene scene(kOddPortrait, {backgroundRemoval()});
		scene.update(scene.backgroundRemoval(), outputDownscaleFactor(example.factor));
		CHECK(scene.width() == example.width);
		CHECK(scene.height() == example.height);
		CHECK(countLogLines(pipelineCreatedLine(1281, 721, example.factor)) >= 1);
	}
}

// MARK: SP-01008 Preceding scale filter

TEST_CASE("SP-01008.1 The downscaled output is based on the preceding filter's size")
{
	for (const SizeExample example : {SizeExample{1, 640, 360}, {2, 320, 180}}) {
		CAPTURE(example.factor);
		Scene scene(kPortrait, {{"scale_filter", R"({"resolution": "640x360"})"}, backgroundRemoval()});
		scene.update(scene.backgroundRemoval(), outputDownscaleFactor(example.factor));
		CHECK(scene.width() == example.width);
		CHECK(scene.height() == example.height);
		CHECK(countLogLines(pipelineCreatedLine(640, 360, example.factor)) >= 1);
		CHECK(backgroundIsRemoved(scene.capture()));
	}
}

// MARK: SP-01009 Preceding crop filter

TEST_CASE("SP-01009.1 The downscaled output is based on the cropped size")
{
	for (const SizeExample example : {SizeExample{1, 1000, 720}, {2, 500, 360}}) {
		CAPTURE(example.factor);
		Scene scene(kPortrait,
			    {{"crop_filter", R"({"relative": true, "left": 140, "right": 140})"}, backgroundRemoval()});
		scene.update(scene.backgroundRemoval(), outputDownscaleFactor(example.factor));
		CHECK(scene.width() == example.width);
		CHECK(scene.height() == example.height);
		CHECK(countLogLines(pipelineCreatedLine(1000, 720, example.factor)) >= 1);
		CHECK(backgroundIsRemoved(scene.capture()));
	}
}

// MARK: SP-01010 Following filter

TEST_CASE("SP-01010.1 The following filter receives the downscaled output")
{
	Scene scene(kPortrait, {backgroundRemoval(), {"color_filter_v2", "{}"}});
	REQUIRE(scene.filter(1) != nullptr);
	scene.update(scene.backgroundRemoval(), outputDownscaleFactor(2));
	CHECK(scene.width() == 640);
	CHECK(scene.height() == 360);
	CHECK(backgroundIsRemoved(scene.capture()));
}

// MARK: SP-01011 Target resized at runtime

TEST_CASE("SP-01011.1 Resizing the preceding filter recreates the pipeline")
{
	Scene scene(kPortrait,
		    {{"scale_filter", R"({"resolution": "640x360"})"}, backgroundRemoval(outputDownscaleFactor(2))});
	REQUIRE(scene.width() == 320);
	REQUIRE(scene.height() == 180);
	clearLog();
	scene.update(scene.filter(0), R"({"resolution": "1280x720"})");
	CHECK(scene.width() == 640);
	CHECK(scene.height() == 360);
	CHECK(countLogLines(pipelineCreatedLine(1280, 720, 2)) == 1);
	CHECK(backgroundIsRemoved(scene.capture()));
}

// MARK: SP-01012 Disabled filter

TEST_CASE("SP-01012.1 Disabling the filter restores the source")
{
	const Image unfiltered = captureUnfiltered(kPortrait);
	Scene scene(kPortrait, {backgroundRemoval(outputDownscaleFactor(2))});
	scene.setEnabled(scene.backgroundRemoval(), false);
	CHECK(scene.width() == 1280);
	CHECK(scene.height() == 720);
	CHECK(maxDifference(scene.capture(), unfiltered) <= 1);
}

TEST_CASE("SP-01012.2 Enabling the filter again draws at once")
{
	Scene scene(kPortrait, {backgroundRemoval(outputDownscaleFactor(2))});
	scene.setEnabled(scene.backgroundRemoval(), false);
	clearLog();
	// Render right after enabling, before the video thread ticks the filter again.
	obs_source_set_enabled(scene.backgroundRemoval(), true);
	CHECK(backgroundIsRemoved(capture(scene.source(), 640, 360)));
	waitFrames(10);
	CHECK_FALSE(anyPipelineCreated());
	CHECK(scene.width() == 640);
	CHECK(scene.height() == 360);
}

TEST_CASE("SP-01012.3 A disabled filter follows changes")
{
	Scene scene(kPortrait, {backgroundRemoval(outputDownscaleFactor(2))});
	scene.setEnabled(scene.backgroundRemoval(), false);
	clearLog();
	scene.update(scene.backgroundRemoval(), outputDownscaleFactor(4));
	CHECK(countLogLines(pipelineCreatedLine(1280, 720, 4)) == 1);
	CHECK(scene.width() == 1280);
	CHECK(scene.height() == 720);
	clearLog();
	scene.setEnabled(scene.backgroundRemoval(), true);
	CHECK_FALSE(anyPipelineCreated());
	CHECK(scene.width() == 320);
	CHECK(scene.height() == 180);
}

// MARK: SP-01013 Shown outside the program output

TEST_CASE("SP-01013.1 Showing but not active")
{
	const Scene scene(kPortrait, {backgroundRemoval()}, Visibility::ShowingOnly);
	REQUIRE(obs_source_showing(scene.source()));
	REQUIRE_FALSE(obs_source_active(scene.source()));
	CHECK(backgroundIsRemoved(capture(scene.source(), 1280, 720)));
}

// MARK: SP-01014 Unknown pipeline

TEST_CASE("SP-01014.1 An unknown pipeline draws nothing")
{
	Scene scene(kPortrait, {backgroundRemoval()});
	clearLog();
	scene.update(scene.backgroundRemoval(), R"({"v2_PipelineId": 9999})");
	CHECK(scene.width() == 0);
	CHECK(scene.height() == 0);
	const Image drawn = capture(scene.source(), 1280, 720);
	CHECK(maxDifference(drawn, Image{1280, 720, std::vector<std::uint8_t>(1280 * 720 * 4)}) == 0);
	CHECK(countLogLines("[obs-backgroundremoval] Unknown rendering pipeline ID: 9999") >= 1);
}

TEST_CASE("SP-01014.2 Selecting Auto recovers")
{
	Scene scene(kPortrait, {backgroundRemoval(R"({"v2_PipelineId": 9999})")});
	REQUIRE(scene.width() == 0);
	scene.update(scene.backgroundRemoval(), R"({"v2_PipelineId": 0})");
	CHECK(scene.width() == 1280);
	CHECK(scene.height() == 720);
	CHECK(backgroundIsRemoved(scene.capture()));
}

// MARK: SP-01015 Repeated downscale factor changes

TEST_CASE("SP-01015.1 Changing the output downscale factor recreates the pipeline")
{
	Scene scene(kPortrait, {backgroundRemoval()});
	waitFrames(300);
	clearLog();
	const int factors[] = {2, 4, 1};
	std::size_t created = 0;
	for (int change = 0; change < 10; ++change) {
		const int factor = factors[change % 3];
		CAPTURE(change);
		scene.update(scene.backgroundRemoval(), outputDownscaleFactor(factor));
		waitFrames(5);
		created += countLogLines(pipelineCreatedLine(1280, 720, factor));
		clearLog();
	}
	CHECK(created == 10);
}
