// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

struct ID3D12Device;

namespace GpuAccessValidation {
bool enabled();
void enable();
void checkMessages(ID3D12Device *device);
} // namespace GpuAccessValidation
