// SPDX-FileCopyrightText: 2025-2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AsyncTextureReader.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace ObsBridgeUtils {

AsyncTextureReader::AsyncTextureReader(std::uint32_t width, std::uint32_t height, gs_color_format format)
	: height_(height),
	  bufferLinesize_(width * getBytesPerPixel(format)),
	  cpuBuffers_{std::vector<std::uint8_t>(static_cast<std::size_t>(height) * bufferLinesize_),
		      std::vector<std::uint8_t>(static_cast<std::size_t>(height) * bufferLinesize_)},
	  stagesurfaces_{makeUniqueStagesurf(width, height, format), makeUniqueStagesurf(width, height, format),
			 makeUniqueStagesurf(width, height, format)}
{
	if (!stagesurfaces_[0] || !stagesurfaces_[1] || !stagesurfaces_[2]) {
		throw std::runtime_error("Failed to create staging surfaces");
	}
}

AsyncTextureReader::~AsyncTextureReader() noexcept = default;

auto AsyncTextureReader::stage(gs_texture_t *texture) noexcept -> bool
{
	if (!texture) {
		return false;
	}

	for (std::size_t offset = 0; offset < stagesurfaces_.size(); ++offset) {
		const std::size_t index = (nextStageIndex_ + offset) % stagesurfaces_.size();
		SlotState expected = SlotState::Free;
		if (slotStates_[index].compare_exchange_strong(expected, SlotState::Busy, std::memory_order_acquire,
							       std::memory_order_relaxed)) {
			gs_stage_texture(stagesurfaces_[index].get(), texture);
			slotSequences_[index].store(nextSequence_++, std::memory_order_relaxed);
			slotStates_[index].store(SlotState::Ready, std::memory_order_release);
			nextStageIndex_ = (index + 1) % stagesurfaces_.size();
			return true;
		}
	}
	return false;
}

auto AsyncTextureReader::sync() -> bool
{
	std::size_t gpuReadIndex = stagesurfaces_.size();
	std::uint64_t newestSequence = 0;
	for (std::size_t index = 0; index < stagesurfaces_.size(); ++index) {
		if (slotStates_[index].load(std::memory_order_acquire) == SlotState::Ready &&
		    (gpuReadIndex == stagesurfaces_.size() ||
		     slotSequences_[index].load(std::memory_order_relaxed) > newestSequence)) {
			gpuReadIndex = index;
			newestSequence = slotSequences_[index].load(std::memory_order_relaxed);
		}
	}
	if (gpuReadIndex == stagesurfaces_.size()) {
		return false;
	}

	SlotState expected = SlotState::Ready;
	if (!slotStates_[gpuReadIndex].compare_exchange_strong(expected, SlotState::Busy, std::memory_order_acquire,
							       std::memory_order_relaxed)) {
		return false;
	}

	for (std::size_t index = 0; index < stagesurfaces_.size(); ++index) {
		if (index != gpuReadIndex && slotStates_[index].load(std::memory_order_acquire) == SlotState::Ready &&
		    slotSequences_[index].load(std::memory_order_relaxed) < newestSequence) {
			expected = SlotState::Ready;
			slotStates_[index].compare_exchange_strong(expected, SlotState::Free, std::memory_order_release,
								   std::memory_order_relaxed);
		}
	}

	std::uint8_t *data = nullptr;
	std::uint32_t linesize = 0;
	gs_stagesurf_t *surface = stagesurfaces_[gpuReadIndex].get();
	if (!gs_stagesurface_map(surface, &data, &linesize) || !data) {
		slotStates_[gpuReadIndex].store(SlotState::Free, std::memory_order_release);
		throw std::runtime_error("gs_stagesurface_map failed");
	}
	const std::size_t backIndex = 1 - activeCpuBufferIndex_.load(std::memory_order_acquire);
	auto &buffer = cpuBuffers_[backIndex];
	if (linesize == bufferLinesize_) {
		std::memcpy(buffer.data(), data, static_cast<std::size_t>(height_) * bufferLinesize_);
	} else {
		for (std::uint32_t y = 0; y < height_; ++y) {
			std::memcpy(buffer.data() + static_cast<std::size_t>(y) * bufferLinesize_,
				    data + static_cast<std::size_t>(y) * linesize, std::min(bufferLinesize_, linesize));
		}
	}
	gs_stagesurface_unmap(surface);
	activeCpuBufferIndex_.store(backIndex, std::memory_order_release);
	slotStates_[gpuReadIndex].store(SlotState::Free, std::memory_order_release);
	return true;
}

auto AsyncTextureReader::getBuffer() const noexcept -> const std::vector<std::uint8_t> &
{
	return cpuBuffers_[activeCpuBufferIndex_.load(std::memory_order_acquire)];
}

auto AsyncTextureReader::getBufferLinesize() const noexcept -> std::uint32_t
{
	return bufferLinesize_;
}

auto AsyncTextureReader::getBytesPerPixel(gs_color_format format) -> std::uint32_t
{
	switch (format) {
	case GS_A8:
	case GS_R8:
		return 1;
	case GS_R8G8:
	case GS_R16:
	case GS_R16F:
		return 2;
	case GS_RGBA:
	case GS_BGRA:
	case GS_BGRX:
	case GS_R10G10B10A2:
	case GS_R32F:
	case GS_RGBA_UNORM:
	case GS_BGRA_UNORM:
	case GS_BGRX_UNORM:
	case GS_RG16:
	case GS_RG16F:
		return 4;
	case GS_RGBA16:
	case GS_RGBA16F:
	case GS_RG32F:
		return 8;
	case GS_RGBA32F:
		return 16;
	default:
		throw std::runtime_error("Unsupported texture format");
	}
}

} // namespace ObsBridgeUtils
