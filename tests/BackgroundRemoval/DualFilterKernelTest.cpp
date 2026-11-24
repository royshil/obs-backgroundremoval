// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

// SP-01002: Dual filter kernel

#include <doctest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "BlurModel.hpp"

using namespace BackgroundRemoval::BlurModel;

namespace
{

constexpr std::uint32_t kFrame = 256;
constexpr std::uint32_t kCenter = kFrame / 2;

// L1 distance between two responses, the second shifted back by (dx, dy).
auto shiftedDistance(const Image &base, const Image &moved, std::uint32_t dx, std::uint32_t dy) -> double
{
    double distance = 0.0;
    for (std::uint32_t y = 0; y < base.height; ++y)
    {
        for (std::uint32_t x = 0; x < base.width; ++x)
        {
            const double other = (x + dx < moved.width && y + dy < moved.height) ? moved.at(x + dx, y + dy) : 0.0;
            distance += std::abs(base.at(x, y) - other);
        }
    }
    return distance;
}

} // namespace

TEST_CASE("SP-01002.1 Impulse response on a power-of-two frame")
{
    struct Example
    {
        std::size_t levels;
        double sigma;
        double anisotropy;
        double shape;
    };
    const Example examples[] = {
        {1, 1.68, 0.05, 0.18}, {2, 3.76, 0.03, 0.09}, {3, 7.71, 0.02, 0.07}, {4, 15.52, 0.01, 0.07}};
    for (const Example &example : examples)
    {
        CAPTURE(example.levels);
        const Image base = dualFilter(impulse(kFrame, kFrame, kCenter, kCenter), example.levels);
        const Moments m = moments(base);
        CHECK(m.sum == doctest::Approx(1.0).epsilon(1e-9));
        CHECK(*std::min_element(base.texels.begin(), base.texels.end()) >= 0.0);
        CHECK(std::abs(m.centerX - (kCenter + 0.5)) < 1e-9);
        CHECK(std::abs(m.centerY - (kCenter + 0.5)) < 1e-9);
        const double sigma = std::sqrt(m.variance());
        CHECK(std::abs(sigma - example.sigma) < 0.01);
        const double diagonal = std::max(std::sqrt(m.varianceU), std::sqrt(m.varianceV));
        CHECK(std::abs(diagonal / sigma - 1.0) <= example.anisotropy);

        const std::uint32_t block = 1u << example.levels;
        double worst = 0.0;
        for (std::uint32_t dy = 0; dy < block; ++dy)
        {
            for (std::uint32_t dx = 0; dx < block; ++dx)
            {
                const Image moved = dualFilter(impulse(kFrame, kFrame, kCenter + dx, kCenter + dy), example.levels);
                worst = std::max(worst, shiftedDistance(base, moved, dx, dy));
            }
        }
        CHECK(worst <= example.shape);
    }
}

TEST_CASE("SP-01002.2 A uniform frame stays uniform")
{
    struct Example
    {
        std::uint32_t width;
        std::uint32_t height;
        std::size_t levels;
    };
    const Example examples[] = {{1280, 720, 5}, {1920, 1080, 5}, {1281, 721, 4}};
    for (const Example &example : examples)
    {
        CAPTURE(example.width);
        CAPTURE(example.height);
        const Image result = dualFilter(Image(example.width, example.height, 1.0), example.levels);
        double worst = 0.0;
        for (const double texel : result.texels)
        {
            worst = std::max(worst, std::abs(texel - 1.0));
        }
        CHECK(worst < 1e-9);
    }
}

TEST_CASE("SP-01002.3 The other reading of halfpixel is rejected")
{
    constexpr std::size_t levels = 4;
    double worst = 0.0;
    for (std::uint32_t phase = 0; phase < (1u << levels); ++phase)
    {
        const Image response =
            dualFilter(impulse(kFrame, kFrame, kCenter + phase, kCenter + phase), levels, HalfPixel::Higher);
        const Moments m = moments(response);
        worst = std::max(worst, std::hypot(m.centerX - (kCenter + phase + 0.5), m.centerY - (kCenter + phase + 0.5)));
    }
    CHECK(worst > 1.0);
}
