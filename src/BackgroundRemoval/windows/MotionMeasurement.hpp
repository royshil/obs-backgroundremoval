// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <optional>

namespace BackgroundRemoval {

// One rendering producer; the UI only reads results or stops the session.
class MotionMeasurement final {
public:
	MotionMeasurement(std::uint64_t startNs, std::uint64_t durationNs) noexcept
		: startNs_(startNs),
		  durationNs_(durationNs)
	{
	}

	void record(std::uint64_t timeNs, double difference) noexcept
	{
		if (!active_.load(std::memory_order_acquire) || timeNs < startNs_ || timeNs - startNs_ >= durationNs_ ||
		    !std::isfinite(difference) || difference < 0.0 || difference > 1.0)
			return;
		maximum_.store(std::max(maximum_.load(std::memory_order_relaxed), difference),
			       std::memory_order_relaxed);
		count_.fetch_add(1, std::memory_order_release);
	}

	void stop() noexcept { active_.store(false, std::memory_order_release); }
	auto sampleCount() const noexcept -> std::uint64_t { return count_.load(std::memory_order_acquire); }
	auto motionSuppression() const noexcept -> std::optional<double>
	{
		if (sampleCount() == 0)
			return std::nullopt;
		const auto maximum = maximum_.load(std::memory_order_relaxed);
		const double sensitivity = maximum == 0.0 ? 100.0 : std::clamp(-20.0 * std::log10(maximum), 0.0, 100.0);
		return std::clamp(60.0 - sensitivity, -50.0, 50.0);
	}

private:
	const std::uint64_t startNs_;
	const std::uint64_t durationNs_;
	std::atomic<bool> active_{true};
	std::atomic<double> maximum_{0.0};
	std::atomic<std::uint64_t> count_{0};
};

} // namespace BackgroundRemoval
