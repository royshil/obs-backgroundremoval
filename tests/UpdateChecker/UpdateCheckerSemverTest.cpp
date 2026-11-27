// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#include "UpdateChecker.hpp"

#include <cstdlib>
#include <iostream>

namespace
{
int failures = 0;

void expect(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void testComponents()
{
    expect(UpdateChecker::parseSemverCore("1.2.3") == UpdateChecker::SemverCore{1, 2, 3},
           "the major, minor and patch components should be returned");
    expect(UpdateChecker::parseSemverCore("18446744073709551615.0.0") ==
               UpdateChecker::SemverCore{18446744073709551615u, 0, 0},
           "a component of the largest uint64 should be accepted");
}

void testPrereleaseAndBuildMetadata()
{
    static constexpr auto version = "10.20.30-rc.1+macos.arm64";
    expect(UpdateChecker::parseSemverCore(version) == UpdateChecker::SemverCore{10, 20, 30},
           "prerelease and build data should not affect the components");
}

void testInvalidVersions()
{
    expect(!UpdateChecker::parseSemverCore("1.2"), "a version without a patch component should be rejected");
    expect(!UpdateChecker::parseSemverCore("01.2.3"), "a core component with a leading zero should be rejected");
    expect(!UpdateChecker::parseSemverCore("invalid"), "a non-version string should be rejected");
    expect(!UpdateChecker::parseSemverCore("18446744073709551616.2.3"),
           "a component outside the uint64 range should be rejected");
}

void testNewerVersion()
{
    expect(UpdateChecker::isSemverNewer("2.0.0", "1.9.9"), "a greater major version should be newer");
    expect(UpdateChecker::isSemverNewer("1.3.0", "1.2.9"), "a greater minor version should be newer");
    expect(UpdateChecker::isSemverNewer("1.2.4", "1.2.3"), "a greater patch version should be newer");
    expect(!UpdateChecker::isSemverNewer("1.2.3", "1.2.3"), "the same version should not be newer");
    expect(!UpdateChecker::isSemverNewer("1.2.2", "1.2.3"), "an older version should not be newer");
    expect(!UpdateChecker::isSemverNewer("invalid", "1.2.3"), "an invalid candidate should not be newer");
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
