// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace AboutDialogIntegration
{

[[nodiscard]] bool show();

// Calls callback on the Qt main thread once no modal dialog is open, so that the About dialog does not compete with
// the Auto-Configuration Wizard, which OBS opens as a modal dialog right after it has started for the first time.
// The wait follows events of Qt and does not poll. Returns false when the main window is unavailable.
[[nodiscard]] bool runWhenNoModalDialog(void (*callback)() noexcept);
// Cancels the call that runWhenNoModalDialog() has scheduled, if it has not run yet.
void cancelRunWhenNoModalDialog() noexcept;

} // namespace AboutDialogIntegration
