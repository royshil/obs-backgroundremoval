// SPDX-FileCopyrightText: 2025-2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "GsUnique.hpp"

namespace ObsBridgeUtils {

class AsyncTextureReader final {
public:
	AsyncTextureReader(std::uint32_t width, std::uint32_t height, gs_color_format format);
	~AsyncTextureReader() noexcept;

	AsyncTextureReader(const AsyncTextureReader &) = delete;
	auto operator=(const AsyncTextureReader &) -> AsyncTextureReader & = delete;

	AsyncTextureReader(AsyncTextureReader &&) = delete;
	auto operator=(AsyncTextureReader &&) -> AsyncTextureReader & = delete;

	auto stage(gs_texture_t *texture) noexcept -> bool;
	auto sync() -> bool;

	auto getBuffer() const noexcept -> const std::vector<std::uint8_t> &;
	auto getBufferLinesize() const noexcept -> std::uint32_t;

private:
	enum class SlotState : std::uint8_t {
		Free,
		Busy,
		Ready,
	};

	static auto getBytesPerPixel(gs_color_format format) -> std::uint32_t;

	const std::uint32_t height_;
	const std::uint32_t bufferLinesize_;
	std::array<std::vector<std::uint8_t>, 2> cpuBuffers_;
	std::atomic<std::size_t> activeCpuBufferIndex_{0};
	const std::array<UniqueStagesurfPtr, 3> stagesurfaces_;
	std::array<std::atomic<SlotState>, 3> slotStates_{SlotState::Free, SlotState::Free, SlotState::Free};
	std::array<std::atomic<std::uint64_t>, 3> slotSequences_{};
	std::size_t nextStageIndex_ = 0;
	std::uint64_t nextSequence_ = 0;
};

} // namespace ObsBridgeUtils
