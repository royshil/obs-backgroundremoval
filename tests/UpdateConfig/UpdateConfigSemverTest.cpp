// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <UpdateConfig/UpdateConfig.hpp>

#include <cstdlib>
#include <iostream>
#include <map>
#include <string_view>

namespace {

void expect(int &failures, bool condition, const char *message)
{
	if (!condition) {
		std::cerr << "FAIL: " << message << '\n';
		++failures;
	}
}

int testComponents()
{
	int failures = 0;
	expect(failures, UpdateConfig::getSemverMajor("1.2.3") == 1, "the major component should be returned");
	expect(failures, UpdateConfig::getSemverMinor("1.2.3") == 2, "the minor component should be returned");
	expect(failures, UpdateConfig::getSemverPatch("1.2.3") == 3, "the patch component should be returned");
	return failures;
}

int testPrereleaseAndBuildMetadata()
{
	int failures = 0;
	static constexpr auto version = "10.20.30-rc.1+macos.arm64";
	expect(failures, UpdateConfig::getSemverMajor(version) == 10,
	       "prerelease data should not affect the major component");
	expect(failures, UpdateConfig::getSemverMinor(version) == 20,
	       "prerelease data should not affect the minor component");
	expect(failures, UpdateConfig::getSemverPatch(version) == 30,
	       "prerelease data should not affect the patch component");
	return failures;
}

int testInvalidVersions()
{
	int failures = 0;
	expect(failures, !UpdateConfig::getSemverMajor("1.2"),
	       "a version without a patch component should be rejected");
	expect(failures, !UpdateConfig::getSemverMinor("01.2.3"),
	       "a core component with a leading zero should be rejected");
	expect(failures, !UpdateConfig::getSemverPatch("invalid"), "a non-version string should be rejected");
	expect(failures, !UpdateConfig::getSemverMajor("18446744073709551616.2.3"),
	       "a component outside the uint64 range should be rejected");
	return failures;
}

int testNewerVersion()
{
	int failures = 0;
	expect(failures, UpdateConfig::isSemverNewer("2.0.0", "1.9.9"), "a greater major version should be newer");
	expect(failures, UpdateConfig::isSemverNewer("1.3.0", "1.2.9"), "a greater minor version should be newer");
	expect(failures, UpdateConfig::isSemverNewer("1.2.4", "1.2.3"), "a greater patch version should be newer");
	expect(failures, !UpdateConfig::isSemverNewer("1.2.3", "1.2.3"), "the same version should not be newer");
	expect(failures, !UpdateConfig::isSemverNewer("1.2.2", "1.2.3"), "an older version should not be newer");
	expect(failures, !UpdateConfig::isSemverNewer("invalid", "1.2.3"), "an invalid candidate should not be newer");
	return failures;
}

const std::map<std::string_view, int (*)()> testFunctions{
	{"testComponents", testComponents},
	{"testInvalidVersions", testInvalidVersions},
	{"testNewerVersion", testNewerVersion},
	{"testPrereleaseAndBuildMetadata", testPrereleaseAndBuildMetadata},
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
