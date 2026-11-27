// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

// SP-01021: Semantic version

#include <doctest.h>

#include <compare>
#include <cstddef>
#include <iterator>
#include <string>
#include <string_view>

#include <PluginGlobal/SemVer.hpp>

using PluginGlobal::SemVer;
using namespace std::string_view_literals;

namespace
{

void checkCleared(const SemVer &semver)
{
    CHECK(semver.majorVersion() == 0);
    CHECK(semver.minorVersion() == 0);
    CHECK(semver.patchVersion() == 0);
    CHECK(semver.preRelease().empty());
    CHECK(semver.build().empty());
}

// Parses str into a version that holds another one, so that clearing it is observable.
void checkRejected(const std::string_view str)
{
    CAPTURE(str);
    SemVer semver;
    REQUIRE(semver.parse("9.9.9-pre+build"));
    CHECK_FALSE(semver.parse(str));
    checkCleared(semver);
}

} // namespace

TEST_CASE("SP-01021.1 A version core is parsed")
{
    struct Example
    {
        std::string_view version;
        long long major;
        long long minor;
        long long patch;
    };
    const Example examples[] = {{"1.23.456", 1, 23, 456}, {"0.0.0", 0, 0, 0}};
    for (const Example &example : examples)
    {
        CAPTURE(example.version);
        SemVer semver;
        REQUIRE(semver.parse(example.version));
        CHECK(semver.majorVersion() == example.major);
        CHECK(semver.minorVersion() == example.minor);
        CHECK(semver.patchVersion() == example.patch);
        CHECK(semver.preRelease().empty());
        CHECK(semver.build().empty());
    }
}

TEST_CASE("SP-01021.2 A pre-release and build metadata are parsed")
{
    struct Example
    {
        std::string_view version;
        std::string_view preRelease;
        std::string_view build;
    };
    const Example examples[] = {
        {"10.20.30-rc.1+macos.arm64", "rc.1", "macos.arm64"},
        {"1.0.0-0.3.7", "0.3.7", ""},
        {"1.0.0+001", "", "001"},
        {"1.0.0-x-y.0a+-", "x-y.0a", "-"},
        {"1.0.0-0", "0", ""},
        {"1.0.0--", "-", ""},
        {"1.0.0-1a.b+0.-", "1a.b", "0.-"},
    };
    for (const Example &example : examples)
    {
        CAPTURE(example.version);
        SemVer semver;
        REQUIRE(semver.parse(example.version));
        CHECK(semver.preRelease() == example.preRelease);
        CHECK(semver.build() == example.build);
    }
}

TEST_CASE("SP-01021.3 A version number has at most 18 digits")
{
    struct Example
    {
        std::string_view accepted;
        std::size_t component;
        std::string_view rejected;
    };
    const Example examples[] = {
        {"999999999999999999.0.0", 0, "1000000000000000000.0.0"},
        {"0.999999999999999999.0", 1, "0.1000000000000000000.0"},
        {"0.0.999999999999999999", 2, "0.0.1000000000000000000"},
    };
    for (const Example &example : examples)
    {
        CAPTURE(example.accepted);
        SemVer semver;
        REQUIRE(semver.parse(example.accepted));
        const long long components[] = {semver.majorVersion(), semver.minorVersion(), semver.patchVersion()};
        for (std::size_t k = 0; k < std::size(components); ++k)
        {
            CHECK(components[k] == (k == example.component ? 999999999999999999LL : 0));
        }
        checkRejected(example.rejected);
    }
}

TEST_CASE("SP-01021.4 A pre-release and build metadata have at most 100 characters")
{
    std::string dotted;
    for (int k = 0; k < 49; ++k)
    {
        dotted += "x.";
    }
    dotted += "xx";
    for (const std::string &text : {std::string(100, 'a'), dotted})
    {
        CAPTURE(text);
        REQUIRE(text.size() == 100);
        SemVer semver;
        REQUIRE(semver.parse("1.0.0-" + text + "+" + text));
        CHECK(semver.preRelease() == text);
        CHECK(semver.build() == text);
        checkRejected("1.0.0-" + text + "x+" + text);
        checkRejected("1.0.0-" + text + "+" + text + "x");
    }
}

TEST_CASE("SP-01021.5 An invalid version is rejected")
{
    for (const std::string_view version :
         {""sv,         "1"sv,          "1.2"sv,        "1.2.3.4"sv,  "1..2.3"sv,     "01.2.3"sv,    "1.02.3"sv,
          "1.2.03"sv,   "v1.2.3"sv,     "-1.2.3"sv,     " 1.2.3"sv,   "1.2.3 "sv,     "1.2.3-"sv,    "1.2.3+"sv,
          "1.2.3-+a"sv, "1.2.3-01"sv,   "1.2.3-a.01"sv, "1.2.3-a."sv, "1.2.3-a..b"sv, "1.2.3-a_b"sv, "1.2.3-\xC3\xA9"sv,
          "1.2.3+.a"sv, "1.2.3+a..b"sv, "1.2.3+a+b"sv})
    {
        checkRejected(version);
    }
}

TEST_CASE("SP-01021.6 A NUL character inside the input is rejected")
{
    for (const std::string_view version : {"1.2.3\0"sv, "1.2.3\0junk"sv, "1.2.3-a\0junk"sv, "1.2.3+a\0junk"sv})
    {
        checkRejected(version);
    }
}

TEST_CASE("SP-01021.7 A copy keeps its own version")
{
    SemVer copy;
    {
        SemVer original;
        REQUIRE(original.parse("1.2.3-rc.1+b.7"));
        copy = original;
        REQUIRE(original.parse("4.5.6-other+other"));
    }
    CHECK(copy.majorVersion() == 1);
    CHECK(copy.minorVersion() == 2);
    CHECK(copy.patchVersion() == 3);
    CHECK(copy.preRelease() == "rc.1");
    CHECK(copy.build() == "b.7");
}

TEST_CASE("SP-01021.8 Versions are ordered by precedence")
{
    // In ascending order of precedence.
    const std::string_view versions[] = {
        "1.0.0-1",        "1.0.0-2",       "1.0.0-10",
        "1.0.0--",        "1.0.0-1a",      "1.0.0-Z",
        "1.0.0-alpha",    "1.0.0-alpha.1", "1.0.0-alpha.beta",
        "1.0.0-alphabet", "1.0.0-beta",    "1.0.0-beta.2",
        "1.0.0-beta.11",  "1.0.0-rc.1",    "1.0.0",
        "1.0.1-0",        "1.0.1",         "1.1.0",
        "2.0.0",          "10.0.0",
    };
    for (std::size_t i = 0; i < std::size(versions); ++i)
    {
        CAPTURE(versions[i]);
        SemVer lhs;
        REQUIRE(lhs.parse(versions[i]));
        for (std::size_t j = 0; j < std::size(versions); ++j)
        {
            CAPTURE(versions[j]);
            SemVer rhs;
            REQUIRE(rhs.parse(versions[j]));
            CHECK(std::is_lt(lhs <=> rhs) == (i < j));
            CHECK(std::is_gt(lhs <=> rhs) == (i > j));
            CHECK((lhs == rhs) == (i == j));
        }
    }
}

TEST_CASE("SP-01021.9 Build metadata does not affect precedence")
{
    struct Example
    {
        std::string_view left;
        std::string_view right;
    };
    const Example examples[] = {
        {"1.0.0+a", "1.0.0+b"},
        {"1.0.0", "1.0.0+build"},
        {"1.0.0-rc.1+a", "1.0.0-rc.1"},
        {"1.0.0-rc.1+a", "1.0.0-rc.1+b"},
    };
    for (const Example &example : examples)
    {
        CAPTURE(example.left);
        CAPTURE(example.right);
        SemVer left;
        SemVer right;
        REQUIRE(left.parse(example.left));
        REQUIRE(right.parse(example.right));
        CHECK(std::is_eq(left <=> right));
        CHECK(left == right);
    }
}

TEST_CASE("SP-01021.10 A version that has not been parsed is cleared")
{
    const SemVer semver;
    checkCleared(semver);
    SemVer zero;
    REQUIRE(zero.parse("0.0.0"));
    CHECK(std::is_eq(semver <=> zero));
    CHECK(semver == zero);
}
