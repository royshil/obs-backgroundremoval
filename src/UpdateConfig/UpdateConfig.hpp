// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace UpdateConfig {

struct HandleFirstRunRequest final {
	std::string pluginVersion;
};

struct SetCheckForUpdatesFlagRequest final {
	bool enabled;
};

struct GetCheckForUpdatesEnabledRequest final {};

using Request = std::variant<HandleFirstRunRequest, SetCheckForUpdatesFlagRequest, GetCheckForUpdatesEnabledRequest>;

[[nodiscard]] bool doRequest(Request &request);

class LatestVersionClient {
public:
	virtual ~LatestVersionClient() = default;

	LatestVersionClient(const LatestVersionClient &) = delete;
	LatestVersionClient &operator=(const LatestVersionClient &) = delete;
	LatestVersionClient(LatestVersionClient &&) = delete;
	LatestVersionClient &operator=(LatestVersionClient &&) = delete;

	virtual void fetchAsync(std::string url, std::string userAgent) = 0;
	virtual std::optional<std::string> getLatestVersion() = 0;
	virtual void reset() noexcept = 0;

protected:
	LatestVersionClient() = default;
};

std::unique_ptr<LatestVersionClient> createLatestVersionClient();
LatestVersionClient &latestVersionClient();

void fetchLatestVersionAsync(std::string url, std::string userAgent);
std::optional<std::string> getLatestVersion();
void resetLatestVersion() noexcept;

#if defined(PLUGIN_NAME_STR) && defined(PLUGIN_VERSION_STR)
inline void fetchLatestVersionAsync(std::string url)
{
	fetchLatestVersionAsync(std::move(url), PLUGIN_NAME_STR "/" PLUGIN_VERSION_STR);
}

#ifdef PLUGIN_LATEST_VERSION_URL
inline void fetchLatestVersionAsync()
{
	fetchLatestVersionAsync(PLUGIN_LATEST_VERSION_URL, PLUGIN_NAME_STR "/" PLUGIN_VERSION_STR);
}
#endif
#endif

} // namespace UpdateConfig
