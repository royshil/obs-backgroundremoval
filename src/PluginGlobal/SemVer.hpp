/*
 * SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <array>
#include <compare>
#include <cstddef>
#include <string_view>

namespace PluginGlobal
{

class SemVer
{
  public:
    struct ComparablePreRelease
    {
        std::string_view str;

        auto operator<=>(const ComparablePreRelease &other) const noexcept
        {
            const std::string_view lhs = str, rhs = other.str;

            if (lhs.empty() && rhs.empty())
            {
                return std::strong_ordering::equal;
            }
            else if (lhs.empty())
            {
                return std::strong_ordering::greater;
            }
            else if (rhs.empty())
            {
                return std::strong_ordering::less;
            }

            std::size_t i = 0, start = 0;

            while (i < lhs.size() && i < rhs.size() && lhs[i] == rhs[i])
            {
                if (lhs[i] == '.')
                {
                    start = i + 1;
                }
                i += 1;
            }

            if (i == lhs.size() && i == rhs.size())
            {
                return std::strong_ordering::equal;
            }
            else if (i == lhs.size() && rhs[i] == '.')
            {
                return std::strong_ordering::less;
            }
            else if (i == rhs.size() && lhs[i] == '.')
            {
                return std::strong_ordering::greater;
            }

            bool prefixIsNum = true;
            for (std::size_t k = start; k < i && prefixIsNum; k += 1)
            {
                if (lhs[k] < '0' || lhs[k] > '9')
                {
                    prefixIsNum = false;
                }
            }

            bool lhsIsNum = prefixIsNum;
            while (i < lhs.size() && lhs[i] != '.')
            {
                if (lhsIsNum && (lhs[i] < '0' || lhs[i] > '9'))
                {
                    lhsIsNum = false;
                }
                i += 1;
            }

            bool rhsIsNum = prefixIsNum;
            std::size_t j = start;
            while (j < rhs.size() && rhs[j] != '.')
            {
                if (rhsIsNum && (rhs[j] < '0' || rhs[j] > '9'))
                {
                    rhsIsNum = false;
                }
                j += 1;
            }

            if (!lhsIsNum && rhsIsNum)
            {
                return std::strong_ordering::greater;
            }
            else if (lhsIsNum && !rhsIsNum)
            {
                return std::strong_ordering::less;
            }
            else if (lhsIsNum && i != j)
            {
                return i <=> j;
            }
            else
            {
                return lhs.substr(start, i - start) <=> rhs.substr(start, j - start);
            }
        }

        bool operator==(const ComparablePreRelease &other) const
        {
            return str == other.str;
        }
    };

    [[nodiscard]] long long majorVersion() const noexcept
    {
        return version[0];
    }

    [[nodiscard]] long long minorVersion() const noexcept
    {
        return version[1];
    }

    [[nodiscard]] long long patchVersion() const noexcept
    {
        return version[2];
    }

    [[nodiscard]] std::string_view preRelease() const noexcept
    {
        return {preReleaseBuf.data(), preReleaseLen};
    }

    [[nodiscard]] ComparablePreRelease comparablePreRelease() const noexcept
    {
        return {preRelease()};
    }

    [[nodiscard]] std::string_view build() const noexcept
    {
        return {buildBuf.data(), buildLen};
    }

    bool parse(const std::string_view str) noexcept;

    auto operator<=>(const SemVer &other) const noexcept
    {
        if (auto cmp = version <=> other.version; cmp != 0)
        {
            return cmp;
        }
        else
        {
            return comparablePreRelease() <=> other.comparablePreRelease();
        }
    }

    bool operator==(const SemVer &other) const noexcept
    {
        return version == other.version && comparablePreRelease() == other.comparablePreRelease();
    }

  private:
    std::array<long long, 3> version{};
    std::array<char, 100> preReleaseBuf{};
    std::size_t preReleaseLen{0};
    std::array<char, 100> buildBuf{};
    std::size_t buildLen{0};
};

} // namespace PluginGlobal
