// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace UpdateChecker
{

// MARK: Versions

// MAJOR, MINOR and PATCH of a semantic version.
using SemverCore = std::array<std::uint64_t, 3>;

// Reads MAJOR.MINOR.PATCH, which may be followed by a prerelease or build suffix starting with - or +.
[[nodiscard]] std::optional<SemverCore> parseSemverCore(std::string_view version);
[[nodiscard]] bool isSemverNewer(std::string_view candidate, std::string_view current);

// MARK: update.ini

// Records version as the plugin version that the user has seen. Returns true only when it differs from
// the recorded version and has been saved, which is when the About dialog is shown on startup.
[[nodiscard]] bool recordPluginVersion(std::string_view version);
[[nodiscard]] bool setCheckForUpdates(bool enabled);
// Whether update checks are enabled, which they are by default. False when update.ini cannot be read.
[[nodiscard]] bool checkForUpdatesEnabled();

} // namespace UpdateChecker
