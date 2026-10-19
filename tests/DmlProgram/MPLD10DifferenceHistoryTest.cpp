// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>
#include <BackgroundRemoval/windows/MediaPipeLandscapeDirectML10/MPLD10DifferenceHistory.hpp>
#include <limits>
#include <thread>

TEST_CASE("Only the last second contributes to the maximum difference boundary")
{
	BackgroundRemoval::MPLD10DifferenceHistory history;
	CHECK_FALSE(history.sensitivity(0));
	history.record(0, 1.0);
	history.record(1000000001ULL, 0.01);
	history.record(1500000000ULL, 0.1);
	auto value = history.sensitivity(2000000000ULL);
	REQUIRE(value);
	CHECK(*value == doctest::Approx(20));
	CHECK(*value == static_cast<float>(-20.0 * std::log10(0.1)));
	CHECK_FALSE(history.sensitivity(2500000001ULL));
}

TEST_CASE("The ring retains exactly sixty values, including after repeated wraparound")
{
	BackgroundRemoval::MPLD10DifferenceHistory history;
	history.record(0, 1.0);
	for (std::uint64_t i = 1; i <= 59; ++i)
		history.record(i, 0.01);
	CHECK(*history.sensitivity(59) == 0);
	history.record(60, 0.01);
	CHECK(*history.sensitivity(60) == doctest::Approx(40));
	for (std::uint64_t i = 61; i < 500; ++i)
		history.record(i, 0.1);
	CHECK(*history.sensitivity(499) == doctest::Approx(20));
}

TEST_CASE("Zero difference, invalid measurements, timestamps, and UI bounds are handled")
{
	BackgroundRemoval::MPLD10DifferenceHistory history;
	history.record(100, std::numeric_limits<double>::quiet_NaN());
	history.record(100, -0.1);
	history.record(100, 1.1);
	CHECK_FALSE(history.sensitivity(100));
	history.record(100, 0);
	CHECK_FALSE(history.sensitivity(99));
	CHECK(*history.sensitivity(100) == 100);
	CHECK(*history.sensitivity(1000000100ULL) == 100);
	CHECK_FALSE(history.sensitivity(1000000101ULL));
	history.record(2000000000ULL, 1e-9);
	CHECK(*history.sensitivity(2000000000ULL) == 100);
}

TEST_CASE("Concurrent publication never overwrites the UI-owned snapshot")
{
	BackgroundRemoval::MPLD10DifferenceHistory history;
	std::atomic<bool> finished{false};
	std::thread producer([&] {
		for (std::uint64_t i = 1; i <= 20000; ++i)
			history.record(i, double(i) / 20000.0);
		finished.store(true, std::memory_order_release);
	});
	float previous = 100;
	while (!finished.load(std::memory_order_acquire)) {
		if (const auto value = history.sensitivity(20000)) {
			CHECK(*value <= previous);
			previous = *value;
		}
	}
	producer.join();
	REQUIRE(history.sensitivity(20000));
	CHECK(*history.sensitivity(20000) == 0);
	// Further reads keep the same completed snapshot until another publication.
	CHECK(*history.sensitivity(20000) == 0);
}
