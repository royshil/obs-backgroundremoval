// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string_view>

// Reads and writes update.ini in the config directory of the plugin, as docs/specs/SP-00011 requires: the version
// that the startup procedure last committed, and whether update checks are enabled.
namespace PluginGlobal::UpdateConfig
{

// Whether NO_STARTUP_DIALOG.txt exists in the config directory of the plugin, which turns the startup dialog off.
[[nodiscard]] bool startupDialogDisabled() noexcept;

// Commits version as the stored version when this is certainly the first startup after an install or an update,
// and returns true only when that commit has succeeded, which is when the startup dialog is shown.
[[nodiscard]] bool commitPluginVersion(std::string_view version) noexcept;

// Commits the setting of update checks, keeping the stored version. Returns false when it was not committed.
[[nodiscard]] bool setCheckForUpdates(bool enabled) noexcept;

// Whether update.ini opens and states check_for_updates as true.
[[nodiscard]] bool checkForUpdatesEnabled() noexcept;

} // namespace PluginGlobal::UpdateConfig
