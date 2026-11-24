// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace BackgroundRemoval
{

// The blur is measured by its variance in texels squared, per axis.
//
// Strength s is the blur of s iterations of a Kawase blur whose iteration i averages four
// diagonal taps at (i + 0.5) texels: each iteration adds (i + 0.5)^2 for the taps and 1/4 for the
// bilinear interpolation between two texels, so s iterations give s (2 s^2 + 1) / 6.
//
// Dual filtering down to level L and back gives (17 / 18) (4^L - 1): level k adds
// (17 / 6) 4^(k - 1). Blending the upsampled result of level k with level k - 1 by a weight
// blends the variances linearly, so level k takes the share of its own variance that the target
// still needs, which is the weight of PS_Upsample in MaskCompositor.cpp with
//   levelOffset = 6 v(k - 1) = (17 / 3) (4^(k - 1) - 1) and levelScale = 1 / (17 4^(k - 1)).
// Both variances are exact for this sampling away from the frame edges, on levels whose sizes halve
// exactly.

/// The deepest blur pyramid the blur ever uses.
inline constexpr std::size_t kMaxBlurLevels = 8;

/// The size of one side of a pyramid level from the size of the level above it: half of it,
/// rounded up, and at least 1.
constexpr auto blurLevelSize(std::uint32_t above) noexcept -> std::uint32_t
{
    return std::max(1u, (above + 1) / 2);
}

/// 4^k.
constexpr auto powerOfFour(std::size_t k) noexcept -> double
{
    double value = 1.0;
    for (std::size_t i = 0; i < k; ++i)
    {
        value *= 4.0;
    }
    return value;
}

/// The variance of the Kawase blur of the given strength.
constexpr auto kawaseVariance(double strength) noexcept -> double
{
    return strength * (2.0 * strength * strength + 1.0) / 6.0;
}

/// The variance of Dual filtering down to the given number of levels and back.
constexpr auto dualVariance(std::size_t levels) noexcept -> double
{
    return 17.0 / 18.0 * (powerOfFour(levels) - 1.0);
}

/// The number of pyramid levels the blur of the given strength needs: the fewest whose variance
/// reaches the strength's, at most kMaxBlurLevels.
constexpr auto blurLevels(double strength) noexcept -> std::size_t
{
    const double variance = kawaseVariance(strength);
    std::size_t levels = 0;
    while (levels < kMaxBlurLevels && dualVariance(levels) < variance)
    {
        ++levels;
    }
    return levels;
}

/// levelOffset of the upsample pass of level k (1-based).
constexpr auto blurLevelOffset(std::size_t level) noexcept -> double
{
    return 17.0 / 3.0 * (powerOfFour(level - 1) - 1.0);
}

/// levelScale of the upsample pass of level k (1-based).
constexpr auto blurLevelScale(std::size_t level) noexcept -> double
{
    return 1.0 / (17.0 * powerOfFour(level - 1));
}

/// The level the upsampling stops at. The final draw blends the frame with it, magnified, by the
/// weight of levels 1 to kBlurStopLevel taken as one. Each upsample pass writes a quarter of the
/// pixels of the one above it, so the two finest passes are most of the upsampling: skipping them
/// saved about 38% of the compositor's GPU time at 3840 x 2160 on Apple silicon, and stopping deeper
/// saved under 5% more while its difference from the full blur started to show.
inline constexpr std::size_t kBlurStopLevel = 2;

/// levelScale of the final draw when the upsampling stops at level k >= 1: levels 1 to k taken as
/// one level, which adds 6 dualVariance(k) = (17 / 3) (4^k - 1) from the frame. It equals
/// blurLevelScale(1) for k = 1.
constexpr auto blurStopScale(std::size_t level) noexcept -> double
{
    return 1.0 / (17.0 / 3.0 * (powerOfFour(level) - 1.0));
}

/// The weight the upsample pass of level k gives the blurred result, for a pixel whose strength is
/// t; the same formula as PS_Upsample in MaskCompositor.cpp.
constexpr auto blurLevelWeight(double strength, std::size_t level) noexcept -> double
{
    const double weight =
        (strength * (2.0 * strength * strength + 1.0) - blurLevelOffset(level)) * blurLevelScale(level);
    return std::clamp(weight, 0.0, 1.0);
}

} // namespace BackgroundRemoval
