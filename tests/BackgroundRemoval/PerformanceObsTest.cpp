// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

// SP-03001: Rendering does not stall, with the filter running in libobs. These cases need a
// hardware GPU, so CTest labels them "performance" and CI leaves them out.

#include <doctest.h>

#include <cstdint>
#include <string>

#include "ObsHarness.hpp"

using namespace ObsHarness;

namespace
{

const std::string kPortrait = testDataFile("portrait-umireon.jpg");

auto backgroundRemoval(const std::string &settingsJson = "{}") -> FilterSpec
{
    return {kBackgroundRemovalId, settingsJson};
}

auto pipelineId(int id) -> std::string
{
    return R"({"v2_PipelineId": )" + std::to_string(id) + "}";
}

} // namespace

// MARK: SP-03001 Rendering does not stall

TEST_CASE("SP-03001.1 Changing the pipeline selection does not stall rendering")
{
    Scene scene(kPortrait, {backgroundRemoval()});
    waitFrames(300);
    for (int change = 0; change < 10; ++change)
    {
        CAPTURE(change);
        const std::uint32_t laggedBefore = obs_get_lagged_frames();
        scene.update(scene.backgroundRemoval(), pipelineId(change % 2 == 0 ? 17 : 0));
        waitFrames(5);
        CHECK(obs_get_lagged_frames() - laggedBefore <= 1);
    }
}

TEST_CASE("SP-03001.2 A second filter does not stall rendering")
{
    Scene scene(kPortrait, {backgroundRemoval()});
    waitFrames(300);
    const std::uint32_t laggedBefore = obs_get_lagged_frames();
    scene.addSecondSource(kPortrait, {backgroundRemoval()});
    waitFrames(30);
    CHECK(obs_get_lagged_frames() - laggedBefore <= 1);
}
