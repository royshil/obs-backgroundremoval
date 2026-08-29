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

void testComponents()
{
	expect(UpdateConfig::getSemverMajor("1.2.3") == 1, "the major component should be returned");
	expect(UpdateConfig::getSemverMinor("1.2.3") == 2, "the minor component should be returned");
	expect(UpdateConfig::getSemverPatch("1.2.3") == 3, "the patch component should be returned");
}

void testPrereleaseAndBuildMetadata()
{
	static constexpr auto version = "10.20.30-rc.1+macos.arm64";
	expect(UpdateConfig::getSemverMajor(version) == 10, "prerelease data should not affect the major component");
	expect(UpdateConfig::getSemverMinor(version) == 20, "prerelease data should not affect the minor component");
	expect(UpdateConfig::getSemverPatch(version) == 30, "prerelease data should not affect the patch component");
}

void testInvalidVersions()
{
	expect(!UpdateConfig::getSemverMajor("1.2"), "a version without a patch component should be rejected");
	expect(!UpdateConfig::getSemverMinor("01.2.3"), "a core component with a leading zero should be rejected");
	expect(!UpdateConfig::getSemverPatch("invalid"), "a non-version string should be rejected");
	expect(!UpdateConfig::getSemverMajor("18446744073709551616.2.3"),
	       "a component outside the uint64 range should be rejected");
}

void testNewerVersion()
{
	expect(UpdateConfig::isSemverNewer("2.0.0", "1.9.9"), "a greater major version should be newer");
	expect(UpdateConfig::isSemverNewer("1.3.0", "1.2.9"), "a greater minor version should be newer");
	expect(UpdateConfig::isSemverNewer("1.2.4", "1.2.3"), "a greater patch version should be newer");
	expect(!UpdateConfig::isSemverNewer("1.2.3", "1.2.3"), "the same version should not be newer");
	expect(!UpdateConfig::isSemverNewer("1.2.2", "1.2.3"), "an older version should not be newer");
	expect(!UpdateConfig::isSemverNewer("invalid", "1.2.3"), "an invalid candidate should not be newer");
}

} // namespace

int main()
{
	testComponents();
	testPrereleaseAndBuildMetadata();
	testInvalidVersions();
	testNewerVersion();
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
