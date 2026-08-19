// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "UpdateConfig.hpp"

namespace UpdateConfig {
namespace {

class DummyLatestVersionClient final : public LatestVersionClient {
public:
	void fetchAsync(std::string, std::string) override {}
	std::optional<std::string> getLatestVersion() override { return {}; }
	void reset() noexcept override {}
};

} // namespace

std::unique_ptr<LatestVersionClient> createLatestVersionClient()
{
	return std::make_unique<DummyLatestVersionClient>();
}

} // namespace UpdateConfig
