// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#include "UpdateConfig.hpp"
#include "test_helpers.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace {
int failures = 0;
using TestHelpers::HttpServer;
using namespace std::chrono_literals;

void expect(bool condition, const char *message)
{
	if (!condition) {
		std::cerr << "FAIL: " << message << '\n';
		++failures;
	}
}

std::optional<std::string> waitForLatestVersion(const std::string &url)
{
	static constexpr auto timeout = 5s;
	const auto deadline = std::chrono::steady_clock::now() + timeout;

	UpdateConfig::fetchLatestVersionAsync(url, "UpdateConfigCurlTest/1.0");
	do {
		if (auto response = UpdateConfig::getLatestVersion()) {
			return response;
		}
		std::this_thread::sleep_for(10ms);
	} while (std::chrono::steady_clock::now() < deadline);

	return {};
}

void testSuccessfulFetch()
{
	HttpServer server("\r\n  1.2.3 \t\n");
	const auto response = waitForLatestVersion(server.url());

	expect(response.has_value(), "a successful fetch should publish its response");
	expect(response == "\r\n  1.2.3", "trailing whitespace should be removed from the response");
	UpdateConfig::resetLatestVersion();
}

void testEmptyResponse()
{
	HttpServer server(" \r\n\t");
	const auto response = waitForLatestVersion(server.url());

	expect(response.has_value(), "a whitespace-only response should be published");
	expect(response == "", "trailing whitespace should be removed from a whitespace-only response");
	UpdateConfig::resetLatestVersion();
}

void testFailedFetch()
{
	HttpServer server("missing", 404);
	UpdateConfig::fetchLatestVersionAsync(server.url(), "UpdateConfigCurlTest/1.0");
	UpdateConfig::resetLatestVersion();
	expect(!UpdateConfig::getLatestVersion(), "a non-success HTTP status should not publish a response");
	UpdateConfig::resetLatestVersion();
}

void testResetAllowsAnotherFetch()
{
	HttpServer firstServer("2.3.4\n");
	expect(waitForLatestVersion(firstServer.url()) == "2.3.4", "the first response should be published");
	UpdateConfig::resetLatestVersion();

	HttpServer secondServer("3.4.5\n");
	expect(waitForLatestVersion(secondServer.url()) == "3.4.5", "reset should allow another fetch");
	UpdateConfig::resetLatestVersion();
}

} // namespace

int main()
{
	UpdateConfig::latestVersionClient();
	testSuccessfulFetch();
	testEmptyResponse();
	testFailedFetch();
	testResetAllowsAnotherFetch();
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
