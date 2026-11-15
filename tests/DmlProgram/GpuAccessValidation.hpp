// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

struct ID3D12Device;

namespace GpuAccessValidation {
bool enabled();
void enable();
void checkMessages(ID3D12Device *device);
} // namespace GpuAccessValidation
