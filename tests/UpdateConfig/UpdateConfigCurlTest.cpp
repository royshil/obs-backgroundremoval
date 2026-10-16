// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <UpdateConfig/UpdateConfig.hpp>
#include "test_helpers.hpp"

#include <obs-module.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <thread>

OBS_DECLARE_MODULE()

namespace {
using TestHelpers::HttpServer;
using namespace std::chrono_literals;

void expect(int &failures, bool condition, const char *message)
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

std::optional<std::string> retryUntilLatestVersion(const std::string &url)
{
	static constexpr auto timeout = 5s;
	const auto deadline = std::chrono::steady_clock::now() + timeout;

	do {
		if (auto response = UpdateConfig::getLatestVersion()) {
			return response;
		}
		UpdateConfig::fetchLatestVersionAsync(url, "UpdateConfigCurlTest/1.0");
		std::this_thread::sleep_for(10ms);
	} while (std::chrono::steady_clock::now() < deadline);

	return {};
}

int testSuccessfulFetch()
{
	int failures = 0;
	UpdateConfig::latestVersionClient();
	HttpServer server("\r\n  1.2.3 \t\n");
	const auto response = waitForLatestVersion(server.url());

	expect(failures, response.has_value(), "a successful fetch should publish its response");
	expect(failures, response == "\r\n  1.2.3", "trailing whitespace should be removed from the response");
	UpdateConfig::resetLatestVersion();
	return failures;
}

int testServerWithoutClientCanBeDestroyed()
{
	UpdateConfig::latestVersionClient();
	HttpServer server("unused");
	return 0;
}

int testEmptyResponse()
{
	int failures = 0;
	UpdateConfig::latestVersionClient();
	HttpServer server(" \r\n\t");
	const auto response = waitForLatestVersion(server.url());

	expect(failures, response.has_value(), "a whitespace-only response should be published");
	expect(failures, response == "", "trailing whitespace should be removed from a whitespace-only response");
	UpdateConfig::resetLatestVersion();
	return failures;
}

int testFailedFetch()
{
	int failures = 0;
	UpdateConfig::latestVersionClient();
	HttpServer server("missing", 404);
	UpdateConfig::fetchLatestVersionAsync(server.url(), "UpdateConfigCurlTest/1.0");
	UpdateConfig::resetLatestVersion();
	expect(failures, !UpdateConfig::getLatestVersion(), "a non-success HTTP status should not publish a response");
	UpdateConfig::resetLatestVersion();
	return failures;
}

int testCompletedFetchAllowsAnotherFetch()
{
	int failures = 0;
	UpdateConfig::latestVersionClient();
	HttpServer firstServer("2.3.4\n");
	expect(failures, waitForLatestVersion(firstServer.url()) == "2.3.4", "the first response should be published");

	HttpServer secondServer("3.4.5\n");
	expect(failures, waitForLatestVersion(secondServer.url()) == "3.4.5",
	       "a completed fetch should allow another fetch");
	UpdateConfig::resetLatestVersion();
	return failures;
}

int testFailedFetchAllowsRetry()
{
	int failures = 0;
	UpdateConfig::latestVersionClient();
	HttpServer failedServer("unavailable", 503, 100ms);
	UpdateConfig::fetchLatestVersionAsync(failedServer.url(), "UpdateConfigCurlTest/1.0");

	HttpServer successfulServer("4.5.6\n");
	expect(failures, retryUntilLatestVersion(successfulServer.url()) == "4.5.6",
	       "a failed fetch should allow a retry");
	UpdateConfig::resetLatestVersion();
	return failures;
}

int testFetchInProgressIgnoresAnotherFetch()
{
	int failures = 0;
	UpdateConfig::latestVersionClient();
	HttpServer server("5.6.7\n", 200, 100ms);
	UpdateConfig::fetchLatestVersionAsync(server.url(), "UpdateConfigCurlTest/1.0");
	UpdateConfig::fetchLatestVersionAsync(server.url(), "UpdateConfigCurlTest/1.0");

	const auto deadline = std::chrono::steady_clock::now() + 5s;
	while (!UpdateConfig::getLatestVersion() && std::chrono::steady_clock::now() < deadline) {
		std::this_thread::sleep_for(10ms);
	}
	expect(failures, UpdateConfig::getLatestVersion() == "5.6.7",
	       "an in-progress fetch should ignore another fetch");
	UpdateConfig::resetLatestVersion();
	return failures;
}

int testNewFetchClearsPreviousResponse()
{
	int failures = 0;
	UpdateConfig::latestVersionClient();
	HttpServer firstServer("6.7.8\n");
	expect(failures, waitForLatestVersion(firstServer.url()) == "6.7.8", "the first response should be published");

	HttpServer failedServer("unavailable", 503, 100ms);
	UpdateConfig::fetchLatestVersionAsync(failedServer.url(), "UpdateConfigCurlTest/1.0");
	expect(failures, !UpdateConfig::getLatestVersion(), "a new fetch should clear the previous response");
	UpdateConfig::resetLatestVersion();
	return failures;
}

const std::map<std::string_view, int (*)()> testFunctions{
	{"testCompletedFetchAllowsAnotherFetch", testCompletedFetchAllowsAnotherFetch},
	{"testEmptyResponse", testEmptyResponse},
	{"testFailedFetch", testFailedFetch},
	{"testFailedFetchAllowsRetry", testFailedFetchAllowsRetry},
	{"testFetchInProgressIgnoresAnotherFetch", testFetchInProgressIgnoresAnotherFetch},
	{"testNewFetchClearsPreviousResponse", testNewFetchClearsPreviousResponse},
	{"testServerWithoutClientCanBeDestroyed", testServerWithoutClientCanBeDestroyed},
	{"testSuccessfulFetch", testSuccessfulFetch},
};

} // namespace

int main(int argc, char **argv)
{
	if (argc != 2) {
		std::cerr << "Usage: " << argv[0] << " <test-name>\n";
		return EXIT_FAILURE;
	}
	const auto test = testFunctions.find(argv[1]);
	if (test == testFunctions.end()) {
		std::cerr << "Unknown test: " << argv[1] << '\n';
		return EXIT_FAILURE;
	}
	return test->second() == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
