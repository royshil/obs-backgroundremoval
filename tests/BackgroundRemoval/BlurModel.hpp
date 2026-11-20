// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// A CPU model of the blur of MaskCompositor, in double precision, with the sampling conventions of
// the GPU: texel i of an axis of N texels has its center at uv (i + 0.5) / N, and textures are
// sampled bilinearly with clamp addressing. The pyramid sizes and the level weights come from the
// plugin itself.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include <BackgroundRemoval/MaskCompositor/BlurLevels.hpp>

namespace BackgroundRemoval::BlurModel {

struct Image {
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	std::vector<double> texels;

	Image(std::uint32_t width_, std::uint32_t height_, double value = 0.0)
		: width(width_),
		  height(height_),
		  texels(static_cast<std::size_t>(width_) * height_, value)
	{
	}

	auto at(std::uint32_t x, std::uint32_t y) const -> double { return texels[std::size_t{y} * width + x]; }
	auto at(std::uint32_t x, std::uint32_t y) -> double & { return texels[std::size_t{y} * width + x]; }
};

inline auto impulse(std::uint32_t width, std::uint32_t height, std::uint32_t x, std::uint32_t y) -> Image
{
	Image image(width, height);
	image.at(x, y) = 1.0;
	return image;
}

/// Samples the image bilinearly at uv with clamp addressing.
inline auto sample(const Image &image, double u, double v) -> double
{
	const double x = std::clamp(u * image.width - 0.5, 0.0, image.width - 1.0);
	const double y = std::clamp(v * image.height - 0.5, 0.0, image.height - 1.0);
	const auto x0 = static_cast<std::uint32_t>(std::floor(x));
	const auto y0 = static_cast<std::uint32_t>(std::floor(y));
	const std::uint32_t x1 = std::min(x0 + 1, image.width - 1);
	const std::uint32_t y1 = std::min(y0 + 1, image.height - 1);
	const double fx = x - x0;
	const double fy = y - y0;
	return (image.at(x0, y0) * (1.0 - fx) + image.at(x1, y0) * fx) * (1.0 - fy) +
	       (image.at(x0, y1) * (1.0 - fx) + image.at(x1, y1) * fx) * fy;
}

/// Renders an image of width x height whose texel at uv is shade(u, v), like a full-target pass.
inline auto render(std::uint32_t width, std::uint32_t height, const std::function<double(double, double)> &shade)
	-> Image
{
	Image output(width, height);
	for (std::uint32_t y = 0; y < height; ++y) {
		for (std::uint32_t x = 0; x < width; ++x) {
			output.at(x, y) = shade((x + 0.5) / width, (y + 0.5) / height);
		}
	}
	return output;
}

/// Which resolution's half texel halfpixel is. The slides of Bjorge, SIGGRAPH 2015 show the lower;
/// the higher is the rejected reading.
enum class HalfPixel { Lower, Higher };

/// PS_Downsample: the center at 1/2 and the four corners at 1/8 each.
inline auto downsample(const Image &above, std::uint32_t width, std::uint32_t height,
		       HalfPixel reading = HalfPixel::Lower) -> Image
{
	const double hx = 0.5 / (reading == HalfPixel::Lower ? width : above.width);
	const double hy = 0.5 / (reading == HalfPixel::Lower ? height : above.height);
	return render(width, height, [&](double u, double v) {
		return (sample(above, u, v) * 4.0 + sample(above, u - hx, v - hy) + sample(above, u + hx, v + hy) +
			sample(above, u + hx, v - hy) + sample(above, u - hx, v + hy)) /
		       8.0;
	});
}

/// The 8-tap ring of PS_Upsample: four diagonal taps at 1/6 and four axial taps twice as far at
/// 1/12.
inline auto upsample(const Image &lower, std::uint32_t width, std::uint32_t height,
		     HalfPixel reading = HalfPixel::Lower) -> Image
{
	const double hx = 0.5 / (reading == HalfPixel::Lower ? lower.width : width);
	const double hy = 0.5 / (reading == HalfPixel::Lower ? lower.height : height);
	return render(width, height, [&](double u, double v) {
		return (sample(lower, u - 2.0 * hx, v) + sample(lower, u - hx, v + hy) * 2.0 +
			sample(lower, u, v + 2.0 * hy) + sample(lower, u + hx, v + hy) * 2.0 +
			sample(lower, u + 2.0 * hx, v) + sample(lower, u + hx, v - hy) * 2.0 +
			sample(lower, u, v - 2.0 * hy) + sample(lower, u - hx, v - hy) * 2.0) /
		       12.0;
	});
}

/// Dual filtering down to the given number of levels and back. The upsample of level k blends with
/// level k - 1 before the blur by weight(k), uniform over the frame; 1 everywhere is plain Dual
/// filtering. Each level is half the size of the one above it, rounded up, as in the plugin.
inline auto dualFilter(const Image &image, std::size_t levels, const std::function<double(std::size_t)> &weight,
		       HalfPixel reading = HalfPixel::Lower) -> Image
{
	std::vector<Image> downs{image};
	for (std::size_t k = 1; k <= levels; ++k) {
		const Image &above = downs.back();
		downs.push_back(downsample(above, blurLevelSize(above.width), blurLevelSize(above.height), reading));
	}
	Image result = downs[levels];
	for (std::size_t k = levels; k >= 1; --k) {
		const Image &unblurred = downs[k - 1];
		Image up = upsample(result, unblurred.width, unblurred.height, reading);
		const double w = weight(k);
		for (std::size_t i = 0; i < up.texels.size(); ++i) {
			up.texels[i] = unblurred.texels[i] + (up.texels[i] - unblurred.texels[i]) * w;
		}
		result = std::move(up);
	}
	return result;
}

inline auto dualFilter(const Image &image, std::size_t levels, HalfPixel reading = HalfPixel::Lower) -> Image
{
	return dualFilter(image, levels, [](std::size_t) { return 1.0; }, reading);
}

/// The blur of the given strength with the plugin's level count and weights, uniform over the frame.
inline auto strengthBlur(const Image &image, double strength) -> Image
{
	return dualFilter(image, blurLevels(strength),
			  [strength](std::size_t k) { return blurLevelWeight(strength, k); });
}

/// The Kawase blur this filter used before: iteration i averages four diagonal taps at (i + 0.5)
/// texels.
inline auto kawase(const Image &image, int iterations) -> Image
{
	Image result = image;
	for (int i = 0; i < iterations; ++i) {
		const double dx = (i + 0.5) / result.width;
		const double dy = (i + 0.5) / result.height;
		const Image source = result;
		result = render(source.width, source.height, [&](double u, double v) {
			return (sample(source, u + dx, v + dy) + sample(source, u - dx, v + dy) +
				sample(source, u + dx, v - dy) + sample(source, u - dx, v - dy)) /
			       4.0;
		});
	}
	return result;
}

/// The moments of an impulse response, in texels.
struct Moments {
	double sum = 0.0;
	double centerX = 0.0;
	double centerY = 0.0;
	double varianceX = 0.0;
	double varianceY = 0.0;
	/// Along the diagonals x = y and x = -y.
	double varianceU = 0.0;
	double varianceV = 0.0;

	auto variance() const -> double { return (varianceX + varianceY) / 2.0; }
};

inline auto moments(const Image &image) -> Moments
{
	Moments m;
	for (std::uint32_t y = 0; y < image.height; ++y) {
		for (std::uint32_t x = 0; x < image.width; ++x) {
			const double w = image.at(x, y);
			m.sum += w;
			m.centerX += w * (x + 0.5);
			m.centerY += w * (y + 0.5);
		}
	}
	m.centerX /= m.sum;
	m.centerY /= m.sum;
	for (std::uint32_t y = 0; y < image.height; ++y) {
		for (std::uint32_t x = 0; x < image.width; ++x) {
			const double w = image.at(x, y);
			const double dx = x + 0.5 - m.centerX;
			const double dy = y + 0.5 - m.centerY;
			m.varianceX += w * dx * dx;
			m.varianceY += w * dy * dy;
			m.varianceU += w * (dx + dy) * (dx + dy) / 2.0;
			m.varianceV += w * (dx - dy) * (dx - dy) / 2.0;
		}
	}
	m.varianceX /= m.sum;
	m.varianceY /= m.sum;
	m.varianceU /= m.sum;
	m.varianceV /= m.sum;
	return m;
}

} // namespace BackgroundRemoval::BlurModel
