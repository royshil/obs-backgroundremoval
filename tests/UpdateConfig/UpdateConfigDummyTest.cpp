// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#include "UpdateConfig.hpp"

#include <cstdlib>
#include <iostream>

namespace {

int failures = 0;

void expect(bool condition, const char *message)
{
	if (!condition) {
		std::cerr << "FAIL: " << message << '\n';
		++failures;
	}
}

} // namespace

int main()
{
	UpdateConfig::latestVersionClient();
	expect(!UpdateConfig::getLatestVersion(), "polling the dummy backend should not return a version");
	UpdateConfig::fetchLatestVersionAsync("https://example.com/latest-version.txt", "DummyTest/1.0");
	expect(!UpdateConfig::getLatestVersion(), "fetching with the dummy backend should not return a version");
	UpdateConfig::resetLatestVersion();
	expect(!UpdateConfig::getLatestVersion(), "resetting the dummy backend should not return a version");

	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
