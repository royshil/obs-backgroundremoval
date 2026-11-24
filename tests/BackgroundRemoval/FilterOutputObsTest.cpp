// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

// SP-01016 to SP-01020: the rendered output of the filter and its settings, running in
// libobs.

#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "ObsHarness.hpp"

using namespace ObsHarness;

namespace
{

const std::string kPortrait = testDataFile("portrait-umireon.jpg");

/// The unfiltered image and the person and background, as the filter with default settings keeps
/// and cuts them.
struct Reference
{
    Image unfiltered;
    std::vector<bool> person;
    std::vector<bool> background;
};

auto makeReference() -> Reference
{
    Reference reference;
    reference.unfiltered = captureUnfiltered(kPortrait);
    const Scene scene(kPortrait, {{kBackgroundRemovalId, "{}"}});
    const Image removed = scene.capture();
    reference.person = foregroundMask(removed);
    reference.background.resize(reference.person.size());
    for (std::size_t i = 0; i < reference.person.size(); ++i)
    {
        reference.background[i] = !reference.person[i];
    }
    return reference;
}

auto blurSettings(double strength, int focusRatio) -> std::string
{
    return R"({"v2_BlurBg": true, "v2_BlurBg_Factor": )" + std::to_string(strength) + R"(, "v2_BlurBg_FocusRatio": )" +
           std::to_string(focusRatio) + "}";
}

auto outputWith(const std::string &settingsJson) -> Image
{
    const Scene scene(kPortrait, {{kBackgroundRemovalId, settingsJson}});
    return scene.capture();
}

/// The pixels within radius of the outline of the person: those whose square window of side
/// 2 radius + 1 holds both the person and the background.
auto outlineBand(const std::vector<bool> &person, std::uint32_t width, std::uint32_t height, int radius)
    -> std::vector<bool>
{
    std::vector<bool> band(person.size());
    const int w = static_cast<int>(width);
    const int h = static_cast<int>(height);
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            const bool here = person[static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)];
            bool mixed = false;
            for (int dy = -radius; dy <= radius && !mixed; ++dy)
            {
                for (int dx = -radius; dx <= radius && !mixed; ++dx)
                {
                    const int nx = std::clamp(x + dx, 0, w - 1);
                    const int ny = std::clamp(y + dy, 0, h - 1);
                    mixed = person[static_cast<std::size_t>(ny) * width + static_cast<std::size_t>(nx)] != here;
                }
            }
            band[static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)] = mixed;
        }
    }
    return band;
}

/// A Color Correction filter that makes the portrait half transparent, ahead of Background Removal.
const FilterSpec kHalfOpacity{"color_filter_v2", R"({"opacity": 0.5})"};

/// The scene with the half transparent portrait, without Background Removal, as OBS composites it.
auto translucentSource() -> Image
{
    const Scene scene(kPortrait, {kHalfOpacity});
    return scene.captureScene();
}

/// The scene with the half transparent portrait through Background Removal with the settings, as
/// OBS composites it.
auto translucentWith(const std::string &settingsJson) -> Image
{
    const Scene scene(kPortrait, {kHalfOpacity, {kBackgroundRemovalId, settingsJson}});
    return scene.captureScene();
}

} // namespace

// MARK: SP-01016 Blur strength

TEST_CASE("SP-01016.1 Strength 0 leaves the frame sharp")
{
    const Image unfiltered = captureUnfiltered(kPortrait);
    const Image output = outputWith(blurSettings(0.0, -100));
    CHECK(opaqueShare(output) == 1.0);
    CHECK(maxDifference(output, unfiltered) <= 1);
}

TEST_CASE("SP-01016.2 The output is opaque")
{
    CHECK(opaqueShare(outputWith(blurSettings(8.0, -100))) == 1.0);
}

TEST_CASE("SP-01016.3 More strength blurs the background more")
{
    const Reference reference = makeReference();
    double previous = -1.0;
    for (const double strength : {2.0, 4.0, 8.0, 20.0})
    {
        CAPTURE(strength);
        const Image output = outputWith(blurSettings(strength, -100));
        const double background = meanChange(output, reference.unfiltered, reference.background);
        const double person = meanChange(output, reference.unfiltered, reference.person);
        CHECK(background > previous);
        CHECK(person < background);
        previous = background;
    }
}

TEST_CASE("SP-01016.4 Strength is continuous")
{
    const Image at4 = outputWith(blurSettings(4.0, -100));
    const Image at41 = outputWith(blurSettings(4.1, -100));
    const Image at5 = outputWith(blurSettings(5.0, -100));
    CHECK(meanChange(at4, at41) < meanChange(at4, at5));
}

// MARK: SP-01017 Blur focus point

TEST_CASE("SP-01017.1 Focus on the person")
{
    const Reference reference = makeReference();
    const Image output = outputWith(blurSettings(8.0, -100));
    CHECK(meanChange(output, reference.unfiltered, reference.person) < 1.0);
    CHECK(meanChange(output, reference.unfiltered, reference.background) > 3.0);
}

TEST_CASE("SP-01017.2 Focus on the background")
{
    const Reference reference = makeReference();
    const Image output = outputWith(blurSettings(8.0, 100));
    CHECK(meanChange(output, reference.unfiltered, reference.background) < 1.0);
    CHECK(meanChange(output, reference.unfiltered, reference.person) > 3.0);
}

TEST_CASE("SP-01017.3 Moving the focus moves the sharpness")
{
    const Reference reference = makeReference();
    double previousPerson = -1.0;
    double previousBackground = 1e9;
    for (const int focusRatio : {-100, 0, 100})
    {
        CAPTURE(focusRatio);
        const Image output = outputWith(blurSettings(8.0, focusRatio));
        const double person = meanChange(output, reference.unfiltered, reference.person);
        const double background = meanChange(output, reference.unfiltered, reference.background);
        CHECK(person > previousPerson);
        CHECK(background < previousBackground);
        previousPerson = person;
        previousBackground = background;
    }
}

TEST_CASE("SP-01017.4 At the balance point the outline is blurred like the rest")
{
    const Reference reference = makeReference();
    const Image output = outputWith(blurSettings(8.0, 0));
    const std::vector<bool> outline =
        outlineBand(reference.person, reference.unfiltered.width, reference.unfiltered.height, 4);
    std::vector<bool> personInside(outline.size());
    std::vector<bool> backgroundInside(outline.size());
    for (std::size_t i = 0; i < outline.size(); ++i)
    {
        personInside[i] = reference.person[i] && !outline[i];
        backgroundInside[i] = reference.background[i] && !outline[i];
    }
    const double atOutline = meanChange(output, reference.unfiltered, outline);
    const double person = meanChange(output, reference.unfiltered, personInside);
    const double background = meanChange(output, reference.unfiltered, backgroundInside);
    CAPTURE(person);
    CAPTURE(background);
    CHECK(atOutline > std::max(person, background));
}

// MARK: SP-01018 Kept pixels

TEST_CASE("SP-01018.1 Kept pixels are unchanged")
{
    const Image unfiltered = captureUnfiltered(kPortrait);
    const Image output = outputWith("{}");
    int worst = 0;
    for (std::size_t i = 0; i < output.pixelCount(); ++i)
    {
        if (output.rgba[i * 4 + 3] != 255)
        {
            continue;
        }
        for (std::size_t c = 0; c < 3; ++c)
        {
            worst = std::max(worst, std::abs(static_cast<int>(output.rgba[i * 4 + c]) -
                                             static_cast<int>(unfiltered.rgba[i * 4 + c])));
        }
    }
    CHECK(worst <= 1);
}

// MARK: SP-01020 Translucent source

TEST_CASE("SP-01020.1 Kept pixels keep their color and transparency")
{
    const Image source = translucentSource();
    const Image output = translucentWith("{}");
    REQUIRE(output.pixelCount() == source.pixelCount());
    std::size_t kept = 0;
    int worst = 0;
    for (std::size_t i = 0; i < output.pixelCount(); ++i)
    {
        if (output.rgba[i * 4 + 3] == 0)
        {
            continue;
        }
        ++kept;
        for (std::size_t c = 0; c < 4; ++c)
        {
            worst = std::max(
                worst, std::abs(static_cast<int>(output.rgba[i * 4 + c]) - static_cast<int>(source.rgba[i * 4 + c])));
        }
    }
    CHECK(kept > output.pixelCount() / 10);
    CHECK(worst <= 2);
}

TEST_CASE("SP-01020.2 Blur strength 0 leaves the source as it is")
{
    const Image source = translucentSource();
    const Image output = translucentWith(blurSettings(0.0, -100));
    CHECK(maxDifference(output, source) <= 2);
}

TEST_CASE("SP-01020.3 The blur keeps the transparency")
{
    const Image source = translucentSource();
    const Image output = translucentWith(blurSettings(8.0, -100));
    REQUIRE(output.pixelCount() == source.pixelCount());
    int worst = 0;
    for (std::size_t i = 0; i < output.pixelCount(); ++i)
    {
        worst = std::max(worst,
                         std::abs(static_cast<int>(output.rgba[i * 4 + 3]) - static_cast<int>(source.rgba[i * 4 + 3])));
    }
    CHECK(worst <= 2);
}
