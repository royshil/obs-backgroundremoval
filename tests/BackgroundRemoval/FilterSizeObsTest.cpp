// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

// SP-01006 to SP-01015: the size, the pipeline lifecycle, and the visibility of the filter,
// running in libobs.

#include <doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "ObsHarness.hpp"

using namespace ObsHarness;

namespace
{

const std::string kPortrait = testDataFile("portrait-umireon.jpg");
const std::string kOddPortrait = testDataFile("portrait-umireon-1281x721.jpg");

auto backgroundRemoval(const std::string &settingsJson = "{}") -> FilterSpec
{
    return {kBackgroundRemovalId, settingsJson};
}

auto pipelineId(int id) -> std::string
{
    return R"({"v2_PipelineId": )" + std::to_string(id) + "}";
}

} // namespace

// MARK: SP-01006 Output size

TEST_CASE("SP-01006.1 The output has the size of the target")
{
    const Scene scene(kPortrait, {backgroundRemoval()});
    CHECK(scene.width() == 1280);
    CHECK(scene.height() == 720);
    CHECK(countLogLines(pipelineCreatedLine(1280, 720)) >= 1);
    CHECK(backgroundIsRemoved(scene.capture()));
}

// MARK: SP-01007 Odd source size

TEST_CASE("SP-01007.1 The output keeps the odd size")
{
    const Scene scene(kOddPortrait, {backgroundRemoval()});
    CHECK(scene.width() == 1281);
    CHECK(scene.height() == 721);
    CHECK(countLogLines(pipelineCreatedLine(1281, 721)) >= 1);
}

// MARK: SP-01008 Preceding scale filter

TEST_CASE("SP-01008.1 The output has the preceding filter's size")
{
    const Scene scene(kPortrait, {{"scale_filter", R"({"resolution": "640x360"})"}, backgroundRemoval()});
    CHECK(scene.width() == 640);
    CHECK(scene.height() == 360);
    CHECK(countLogLines(pipelineCreatedLine(640, 360)) >= 1);
    CHECK(backgroundIsRemoved(scene.capture()));
}

// MARK: SP-01009 Preceding crop filter

TEST_CASE("SP-01009.1 The output has the cropped size")
{
    const Scene scene(kPortrait,
                      {{"crop_filter", R"({"relative": true, "left": 140, "right": 140})"}, backgroundRemoval()});
    CHECK(scene.width() == 1000);
    CHECK(scene.height() == 720);
    CHECK(countLogLines(pipelineCreatedLine(1000, 720)) >= 1);
    CHECK(backgroundIsRemoved(scene.capture()));
}

// MARK: SP-01010 Following filter

TEST_CASE("SP-01010.1 The following filter receives the output")
{
    const Scene scene(kPortrait, {backgroundRemoval(), {"color_filter_v2", "{}"}});
    REQUIRE(scene.filter(1) != nullptr);
    CHECK(scene.width() == 1280);
    CHECK(scene.height() == 720);
    CHECK(backgroundIsRemoved(scene.capture()));
}

// MARK: SP-01011 Target resized at runtime

TEST_CASE("SP-01011.1 Resizing the preceding filter recreates the pipeline")
{
    Scene scene(kPortrait, {{"scale_filter", R"({"resolution": "640x360"})"}, backgroundRemoval()});
    REQUIRE(scene.width() == 640);
    REQUIRE(scene.height() == 360);
    clearLog();
    scene.update(scene.filter(0), R"({"resolution": "1280x720"})");
    CHECK(scene.width() == 1280);
    CHECK(scene.height() == 720);
    CHECK(countLogLines(pipelineCreatedLine(1280, 720)) == 1);
    CHECK(backgroundIsRemoved(scene.capture()));
}

// MARK: SP-01012 Disabled filter

TEST_CASE("SP-01012.1 Disabling the filter restores the source")
{
    const Image unfiltered = captureUnfiltered(kPortrait);
    Scene scene(kPortrait, {backgroundRemoval()});
    scene.setEnabled(scene.backgroundRemoval(), false);
    CHECK(scene.width() == 1280);
    CHECK(scene.height() == 720);
    CHECK(maxDifference(scene.capture(), unfiltered) <= 1);
}

TEST_CASE("SP-01012.2 Enabling the filter again draws at once")
{
    Scene scene(kPortrait, {backgroundRemoval()});
    scene.setEnabled(scene.backgroundRemoval(), false);
    clearLog();
    // Render right after enabling, before the video thread ticks the filter again.
    obs_source_set_enabled(scene.backgroundRemoval(), true);
    CHECK(backgroundIsRemoved(capture(scene.source(), 1280, 720)));
    waitFrames(10);
    CHECK_FALSE(anyPipelineCreated());
    CHECK(scene.width() == 1280);
    CHECK(scene.height() == 720);
}

TEST_CASE("SP-01012.3 A disabled filter follows changes")
{
    Scene scene(kPortrait, {backgroundRemoval()});
    scene.setEnabled(scene.backgroundRemoval(), false);
    clearLog();
    scene.update(scene.backgroundRemoval(), pipelineId(17));
    CHECK(countLogLines(pipelineCreatedLine(1280, 720)) == 1);
    CHECK(scene.width() == 1280);
    CHECK(scene.height() == 720);
    clearLog();
    scene.setEnabled(scene.backgroundRemoval(), true);
    CHECK_FALSE(anyPipelineCreated());
    CHECK(scene.width() == 1280);
    CHECK(scene.height() == 720);
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

// MARK: SP-01015 Repeated pipeline selection changes

TEST_CASE("SP-01015.1 Changing the pipeline selection recreates the pipeline")
{
    Scene scene(kPortrait, {backgroundRemoval()});
    clearLog();
    std::size_t created = 0;
    for (int change = 0; change < 10; ++change)
    {
        CAPTURE(change);
        scene.update(scene.backgroundRemoval(), pipelineId(change % 2 == 0 ? 17 : 0));
        created += countLogLines(pipelineCreatedLine(1280, 720));
        clearLog();
    }
    CHECK(created == 10);
}
