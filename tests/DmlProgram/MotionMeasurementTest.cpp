// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>
#include <BackgroundRemoval/windows/MotionMeasurement.hpp>
#include <limits>
#include <thread>

using BackgroundRemoval::MotionMeasurement;

TEST_CASE("Results are inaccessible until the video thread publishes completion")
{
	MotionMeasurement measurement(100);
	measurement.requestStart();
	CHECK_FALSE(measurement.readable());
	measurement.tick(100, 0.01);
	CHECK_FALSE(measurement.motionSuppression());
	measurement.tick(199, 0.1);
	measurement.tick(200, 1.0);
	REQUIRE(measurement.readable());
	CHECK(measurement.samples().size() == 2);
	CHECK(*measurement.motionSuppression() == doctest::Approx(40.0));
	measurement.tick(201, 1.0);
	CHECK(measurement.samples().size() == 2);
}

TEST_CASE("Cancel waits for the writer and an immediate restart cannot read old results")
{
	MotionMeasurement measurement(100);
	measurement.requestStart();
	measurement.tick(100, 0.01);
	measurement.requestCancel();
	CHECK_FALSE(measurement.readable());
	measurement.tick(101, 1.0);
	REQUIRE(measurement.readable());
	CHECK(measurement.samples().size() == 1);
	measurement.requestStart();
	CHECK_FALSE(measurement.readable());
	measurement.tick(200, 0.0);
	CHECK_FALSE(measurement.readable());
	measurement.tick(300);
	REQUIRE(measurement.readable());
	CHECK(measurement.samples().size() == 1);
	CHECK(*measurement.motionSuppression() == -40.0);
}

TEST_CASE("Invalid samples and missing input produce no recommendation")
{
	MotionMeasurement measurement(100);
	measurement.requestStart();
	measurement.tick(0, -0.1);
	measurement.tick(1, 1.1);
	measurement.tick(2, std::numeric_limits<double>::infinity());
	measurement.tick(3);
	measurement.tick(100);
	REQUIRE(measurement.readable());
	CHECK(measurement.samples().empty());
	CHECK_FALSE(measurement.motionSuppression());
}

TEST_CASE("UI and video thread hand off the vector across repeated sessions")
{
	MotionMeasurement measurement(10);
	std::atomic<bool> stop{false};
	std::thread writer([&] {
		std::uint64_t now = 0;
		while (!stop.load(std::memory_order_acquire))
			measurement.tick(now++, 0.01);
	});
	bool valid = true;
	for (int session = 0; session < 1000; ++session) {
		measurement.requestStart();
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
		while (!measurement.readable() && std::chrono::steady_clock::now() < deadline)
			std::this_thread::yield();
		if (!measurement.readable()) {
			valid = false;
			break;
		}
		valid &= measurement.samples().size() == 10;
		valid &= measurement.motionSuppression().has_value();
	}
	stop.store(true, std::memory_order_release);
	writer.join();
	CHECK(valid);
}
