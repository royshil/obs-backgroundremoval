// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>
#include <BackgroundRemoval/windows/MotionMeasurement.hpp>
#include <limits>

TEST_CASE("Only samples submitted during the measurement window contribute")
{
	BackgroundRemoval::MotionMeasurement measurement(100, 100);
	measurement.record(99, 1.0);
	CHECK_FALSE(measurement.motionSuppression());
	measurement.record(100, 0.01);
	measurement.record(199, 0.1);
	measurement.record(200, 1.0);
	CHECK(measurement.sampleCount() == 2);
	REQUIRE(measurement.motionSuppression());
	CHECK(*measurement.motionSuppression() == doctest::Approx(40.0));
}

TEST_CASE("Stopping excludes subsequent samples and preserves the result")
{
	BackgroundRemoval::MotionMeasurement measurement(0, 100);
	measurement.record(1, 0.01);
	measurement.stop();
	measurement.record(2, 1.0);
	CHECK(measurement.sampleCount() == 1);
	CHECK(*measurement.motionSuppression() == doctest::Approx(20.0));
}

TEST_CASE("No input and invalid input do not produce a recommendation")
{
	BackgroundRemoval::MotionMeasurement measurement(0, 100);
	measurement.record(1, -0.1);
	measurement.record(1, 1.1);
	measurement.record(1, std::numeric_limits<double>::quiet_NaN());
	measurement.record(1, std::numeric_limits<double>::infinity());
	CHECK(measurement.sampleCount() == 0);
	CHECK_FALSE(measurement.motionSuppression());
}

TEST_CASE("Zero motion is valid and larger differences need more suppression")
{
	BackgroundRemoval::MotionMeasurement measurement(0, 100);
	measurement.record(1, 0.0);
	CHECK(*measurement.motionSuppression() == -40.0);
	measurement.record(2, 0.01);
	CHECK(*measurement.motionSuppression() == doctest::Approx(20.0));
	measurement.record(3, 1.0);
	CHECK(*measurement.motionSuppression() == 50.0);
}
