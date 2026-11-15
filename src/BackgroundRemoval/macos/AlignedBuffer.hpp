// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

namespace Memory {

struct AlignedByteDeleter final {
	std::size_t alignment = 32;

	void operator()(std::uint8_t *data) const noexcept;
};

using AlignedBufferPtr = std::unique_ptr<std::uint8_t[], AlignedByteDeleter>;

auto makeAlignedBytes(std::size_t size, std::size_t alignment = 32) -> AlignedBufferPtr;

} // namespace Memory
