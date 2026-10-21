// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#ifndef PLUGIN_PRIVATE
#define PLUGIN_PRIVATE private
#endif

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace BackgroundRemoval {

class MotionMeasurement final {
public:
	enum class Request { None, Start, Cancel };

	explicit MotionMeasurement(std::uint64_t durationNs = 3000000000) noexcept : durationNs_(durationNs) {}

	// UI thread only. Finish reading the previous result before requesting a write.
	void requestStart() noexcept { motionMeasurementRequest_.store(Request::Start, std::memory_order_release); }
	void requestCancel() noexcept { motionMeasurementRequest_.store(Request::Cancel, std::memory_order_release); }

	// The request acknowledgement precedes the readable load, so a previous
	// session's true flag cannot be mistaken for completion of a new request.
	auto readable() const noexcept -> bool
	{
		return motionMeasurementRequest_.load(std::memory_order_acquire) == Request::None &&
		       readable_.load(std::memory_order_acquire);
	}

	// OBS video thread only. No UI access to samples until readable() is true.
	void tick(std::uint64_t timeNs, double motion = std::numeric_limits<double>::quiet_NaN()) noexcept
	{
		if (motionMeasurementRequest_.load(std::memory_order_acquire) != Request::None) {
			readable_.store(false, std::memory_order_relaxed);
			const auto request =
				motionMeasurementRequest_.exchange(Request::None, std::memory_order_acq_rel);
			if (request == Request::Start) {
				samples_.clear();
				startNs_ = timeNs;
				writing_ = true;
			} else if (request == Request::Cancel) {
				writing_ = false;
				readable_.store(true, std::memory_order_release);
			}
		}
		if (!writing_)
			return;
		if (timeNs >= startNs_ && timeNs - startNs_ >= durationNs_) {
			writing_ = false;
			readable_.store(true, std::memory_order_release);
			return;
		}
		if (timeNs < startNs_ || !std::isfinite(motion) || motion < 0.0 || motion > 1.0)
			return;
		try {
			samples_.push_back(motion);
		} catch (...) {
			// Publish an empty result if allocation fails; never escape videoTick.
			samples_.clear();
			writing_ = false;
			readable_.store(true, std::memory_order_release);
		}
	}

	// UI thread only, after readable(), and before the next request.
	auto samples() const noexcept -> const std::vector<double> & { return samples_; }
	auto motionSuppression() const noexcept -> std::optional<double>
	{
		if (!readable() || samples_.empty())
			return std::nullopt;
		const auto maximum = *std::max_element(samples_.begin(), samples_.end());
		const double sensitivity = maximum == 0.0 ? 100.0 : std::clamp(-20.0 * std::log10(maximum), 0.0, 100.0);
		return std::clamp(60.0 - sensitivity, -50.0, 50.0);
	}

PLUGIN_PRIVATE:
	std::atomic<Request> motionMeasurementRequest_{Request::None};
	std::atomic<bool> readable_{false};
	std::vector<double> samples_;
	const std::uint64_t durationNs_;
	std::uint64_t startNs_ = 0;
	bool writing_ = false;
};

} // namespace BackgroundRemoval
