// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#include "LandscapeTestModel.hpp"
#include <doctest.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <winrt/base.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <vector>

namespace BackgroundRemoval::PipelineTest {
auto portraitInput() -> std::vector<float>;
}

TEST_CASE("PortraitFixture")
{
	using Microsoft::WRL::ComPtr;
	ComPtr<IWICImagingFactory> factory;
	winrt::check_hresult(
		CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)));
	ComPtr<IWICBitmapDecoder> decoder;
	winrt::check_hresult(factory->CreateDecoderFromFilename(winrt::to_hstring(LANDSCAPE_CUTOUT).c_str(), nullptr,
								GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder));
	ComPtr<IWICBitmapFrameDecode> frame;
	winrt::check_hresult(decoder->GetFrame(0, &frame));
	UINT width = 0, height = 0;
	winrt::check_hresult(frame->GetSize(&width, &height));
	REQUIRE(width == 1280);
	REQUIRE(height == 720);
	ComPtr<IWICFormatConverter> converter;
	winrt::check_hresult(factory->CreateFormatConverter(&converter));
	winrt::check_hresult(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
						   nullptr, 0, WICBitmapPaletteTypeCustom));
	std::vector<std::uint8_t> rgba(width * height * 4);
	winrt::check_hresult(converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(rgba.size()), rgba.data()));

	const auto input = BackgroundRemoval::PipelineTest::portraitInput();
	auto runtime = LandscapeTestModel::makeMaskRuntime();
	// Frozen fixture was visually accepted with a model threshold of 0.42.
	const float threshold = 0.42F;
	const std::array inputs{std::as_bytes(std::span(input)), std::as_bytes(std::span(&threshold, 1))};
	std::vector<float> background(256 * 144);
	runtime->run(inputs, std::as_writable_bytes(std::span(background)));
	bool finite = true;
	for (const auto value : background)
		finite &= std::isfinite(value) && value >= 0 && value <= 1;
	REQUIRE(finite);
	auto at = [&](int x, int y) {
		return background[std::clamp(y, 0, 143) * 256 + std::clamp(x, 0, 255)];
	};
	std::size_t intersection = 0, unionCount = 0, disagreement = 0, foreground = 0;
	bool binaryAlpha = true;
	for (UINT y = 0; y < height; ++y)
		for (UINT x = 0; x < width; ++x) {
			const float sx = (float(x) + 0.5F) * (256.0F / width) - 0.5F;
			const float sy = (float(y) + 0.5F) * (144.0F / height) - 0.5F;
			const int ix = int(std::floor(sx)), iy = int(std::floor(sy));
			const float fx = sx - ix, fy = sy - iy;
			const float top = at(ix, iy) + (at(ix + 1, iy) - at(ix, iy)) * fx;
			const float bottom = at(ix, iy + 1) + (at(ix + 1, iy + 1) - at(ix, iy + 1)) * fx;
			const bool actual = (top + (bottom - top) * fy) <= 0.5F;
			const auto alpha = rgba[(y * width + x) * 4 + 3];
			binaryAlpha &= alpha == 0 || alpha == 255;
			const bool expected = alpha == 255;
			foreground += expected;
			intersection += actual && expected;
			unionCount += actual || expected;
			disagreement += actual != expected;
		}
	REQUIRE(binaryAlpha);
	REQUIRE(foreground > 0);
	REQUIRE(foreground < std::size_t(width) * height);
	REQUIRE(unionCount > 0);
	const double iou = double(intersection) / double(unionCount);
	const double mismatch = double(disagreement) / double(width * height);
	std::cout << "PortraitFixture: IoU=" << iou << ", pixel_mismatch=" << mismatch << '\n';
	CAPTURE(iou);
	CAPTURE(mismatch);
	// Allow at most 0.5% foreground-union drift and 0.2% full-frame drift.
	CHECK(iou >= 0.995);
	CHECK(mismatch <= 0.002);
}
