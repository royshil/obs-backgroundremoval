// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

struct obs_properties;
using obs_properties_t = struct obs_properties;

namespace AboutDialogIntegration {

[[nodiscard]] bool show();
void addButton(obs_properties_t *properties);

} // namespace AboutDialogIntegration
