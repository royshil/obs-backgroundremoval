// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "BackgroundRemoval/AlignedBuffer.hpp"

#include <new>

namespace Memory {

void AlignedByteDeleter::operator()(std::uint8_t *data) const noexcept
{
	::operator delete[](data, std::align_val_t{alignment});
}

auto makeAlignedBytes(std::size_t size, std::size_t alignment) -> AlignedBufferPtr
{
	auto *data = static_cast<std::uint8_t *>(::operator new[](size, std::align_val_t{alignment}));
	return AlignedBufferPtr(data, AlignedByteDeleter{alignment});
}

} // namespace Memory
