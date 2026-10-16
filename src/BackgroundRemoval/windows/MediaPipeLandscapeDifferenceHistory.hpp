// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace BackgroundRemoval {

class MediaPipeLandscapeDifferenceHistory final {
public:
	void record(std::uint64_t timeNs, double difference) noexcept
	{
		if (!std::isfinite(difference) || difference < 0.0 || difference > 1.0)
			return;
		samples_[writeIndex_][next_] = {timeNs, difference};
		next_ = (next_ + 1) % samples_[writeIndex_].size();
		count_ = std::min(count_ + 1, samples_[writeIndex_].size());
		counts_[writeIndex_] = count_;
		const auto published = writeIndex_;
		writeIndex_ =
			bufferIndex(sharedState_.exchange(kPublishedStates[published], std::memory_order_acq_rel));
		// The new writer slot is no longer owned by the reader. Continue its ring from this snapshot.
		samples_[writeIndex_] = samples_[published];
	}

	auto sensitivity(std::uint64_t nowNs) noexcept -> std::optional<float>
	{
		// One producer (rendering) and one consumer (UI). The reader keeps its slot until the next exchange.
		if (sharedState_.load(std::memory_order_acquire) >= BufferState::Published0)
			readIndex_ = bufferIndex(
				sharedState_.exchange(kReturnedStates[readIndex_], std::memory_order_acq_rel));
		bool found = false;
		double maximum = 0.0;
		for (std::size_t i = 0; i < counts_[readIndex_]; ++i) {
			const auto &sample = samples_[readIndex_][i];
			if (sample.timeNs > nowNs || nowNs - sample.timeNs > 1000000000ULL)
				continue;
			found = true;
			maximum = std::max(maximum, sample.difference);
		}
		if (!found)
			return std::nullopt;
		if (maximum == 0.0)
			return 100.0F;
		return std::clamp(static_cast<float>(-20.0 * std::log10(maximum)), 0.0F, 100.0F);
	}

private:
	struct Sample {
		std::uint64_t timeNs = 0;
		double difference = 0.0;
	};
	std::array<std::array<Sample, 60>, 3> samples_{};
	std::array<std::size_t, 3> counts_{};
	// The single shared flag identifies the exchange slot and whether it has a new snapshot.
	enum class BufferState : std::uint32_t {
		Returned0,
		Returned1,
		Returned2,
		Published0,
		Published1,
		Published2,
	};
	static constexpr std::array kReturnedStates{BufferState::Returned0, BufferState::Returned1,
						    BufferState::Returned2};
	static constexpr std::array kPublishedStates{BufferState::Published0, BufferState::Published1,
						     BufferState::Published2};
	static constexpr auto bufferIndex(BufferState state) noexcept -> std::uint32_t
	{
		switch (state) {
		case BufferState::Returned0:
		case BufferState::Published0:
			return 0;
		case BufferState::Returned1:
		case BufferState::Published1:
			return 1;
		case BufferState::Returned2:
		case BufferState::Published2:
			return 2;
		}
		return 0;
	}
	static_assert(std::atomic<BufferState>::is_always_lock_free);
	std::atomic<BufferState> sharedState_{BufferState::Returned2};
	std::uint32_t writeIndex_ = 1;
	std::uint32_t readIndex_ = 0;
	std::size_t next_ = 0;
	std::size_t count_ = 0;
};

} // namespace BackgroundRemoval
