// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

// SP-01016 to SP-01019: the rendered output of the filter and its settings, running in
// libobs.

#include <doctest.h>

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include "ObsHarness.hpp"

using namespace ObsHarness;

namespace {

const std::string kPortrait = testDataFile("portrait-umireon.jpg");

/// The unfiltered image and the person and background, as the filter with default settings keeps
/// and cuts them.
struct Reference {
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
	for (std::size_t i = 0; i < reference.person.size(); ++i) {
		reference.background[i] = !reference.person[i];
	}
	return reference;
}

auto blurSettings(double strength, double focusPoint) -> std::string
{
	return R"({"v2_BlurBg": true, "v2_BlurBg_Factor": )" + std::to_string(strength) +
	       R"(, "v2_BlurBg_FocusPoint": )" + std::to_string(focusPoint) + "}";
}

auto outputWith(const std::string &settingsJson) -> Image
{
	const Scene scene(kPortrait, {{kBackgroundRemovalId, settingsJson}});
	return scene.capture();
}

} // namespace

// MARK: SP-01016 Blur strength

TEST_CASE("SP-01016.1 Strength 0 leaves the frame sharp")
{
	const Image unfiltered = captureUnfiltered(kPortrait);
	const Image output = outputWith(blurSettings(0.0, 0.1));
	CHECK(opaqueShare(output) == 1.0);
	CHECK(maxDifference(output, unfiltered) <= 1);
}

TEST_CASE("SP-01016.2 The output is opaque")
{
	CHECK(opaqueShare(outputWith(blurSettings(8.0, 0.1))) == 1.0);
}

TEST_CASE("SP-01016.3 More strength blurs the background more")
{
	const Reference reference = makeReference();
	double previous = -1.0;
	for (const double strength : {2.0, 4.0, 8.0, 20.0}) {
		CAPTURE(strength);
		const Image output = outputWith(blurSettings(strength, 0.1));
		const double background = meanChange(output, reference.unfiltered, reference.background);
		const double person = meanChange(output, reference.unfiltered, reference.person);
		CHECK(background > previous);
		CHECK(person < background);
		previous = background;
	}
}

TEST_CASE("SP-01016.4 Strength is continuous")
{
	const Image at4 = outputWith(blurSettings(4.0, 0.1));
	const Image at41 = outputWith(blurSettings(4.1, 0.1));
	const Image at5 = outputWith(blurSettings(5.0, 0.1));
	CHECK(meanChange(at4, at41) < meanChange(at4, at5));
}

// MARK: SP-01017 Blur focus point

TEST_CASE("SP-01017.1 Focus on the person")
{
	const Reference reference = makeReference();
	const Image output = outputWith(blurSettings(8.0, 0.0));
	CHECK(meanChange(output, reference.unfiltered, reference.person) < 1.0);
	CHECK(meanChange(output, reference.unfiltered, reference.background) > 3.0);
}

TEST_CASE("SP-01017.2 Focus on the background")
{
	const Reference reference = makeReference();
	const Image output = outputWith(blurSettings(8.0, 1.0));
	CHECK(meanChange(output, reference.unfiltered, reference.background) < 1.0);
	CHECK(meanChange(output, reference.unfiltered, reference.person) > 3.0);
}

TEST_CASE("SP-01017.3 Moving the focus moves the sharpness")
{
	const Reference reference = makeReference();
	double previousPerson = -1.0;
	double previousBackground = 1e9;
	for (const double focusPoint : {0.0, 0.5, 1.0}) {
		CAPTURE(focusPoint);
		const Image output = outputWith(blurSettings(8.0, focusPoint));
		const double person = meanChange(output, reference.unfiltered, reference.person);
		const double background = meanChange(output, reference.unfiltered, reference.background);
		CHECK(person > previousPerson);
		CHECK(background < previousBackground);
		previousPerson = person;
		previousBackground = background;
	}
}

// MARK: SP-01018 Kept pixels

TEST_CASE("SP-01018.1 Kept pixels are unchanged")
{
	const Image unfiltered = captureUnfiltered(kPortrait);
	const Image output = outputWith("{}");
	int worst = 0;
	for (std::size_t i = 0; i < output.pixelCount(); ++i) {
		if (output.rgba[i * 4 + 3] != 255) {
			continue;
		}
		for (std::size_t c = 0; c < 3; ++c) {
			worst = std::max(worst, std::abs(static_cast<int>(output.rgba[i * 4 + c]) -
							 static_cast<int>(unfiltered.rgba[i * 4 + c])));
		}
	}
	CHECK(worst <= 1);
}

// MARK: SP-01019 Settings compatibility

TEST_CASE("SP-01019.1 An integer blur strength is read as the same strength")
{
	const Image saved = outputWith(R"({"v2_BlurBg": true, "v2_BlurBg_Factor": 8})");
	const Image current = outputWith(R"({"v2_BlurBg": true, "v2_BlurBg_Factor": 8.0})");
	CHECK(maxDifference(saved, current) == 0);
}

TEST_CASE("SP-01019.2 The removed focus depth is ignored")
{
	const Image saved = outputWith(R"({"v2_BlurBg": true, "v2_BlurBg_FocusDepth": 0.2})");
	const Image current = outputWith(R"({"v2_BlurBg": true})");
	CHECK(maxDifference(saved, current) == 0);
}

TEST_CASE("SP-01019.3 Settings without the output downscale factor use downscale factor 1")
{
	const Scene scene(kPortrait, {{kBackgroundRemovalId, "{}"}});
	CHECK(scene.width() == 1280);
	CHECK(scene.height() == 720);
}

TEST_CASE("SP-01019.4 The removed mask adjustment is ignored")
{
	const Image saved = outputWith(R"({"v2_MaskAdjustment": 30})");
	const Image current = outputWith("{}");
	CHECK(maxDifference(saved, current) == 0);
}
