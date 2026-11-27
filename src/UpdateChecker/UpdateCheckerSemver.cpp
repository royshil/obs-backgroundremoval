// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "UpdateChecker.hpp"

#include <cstddef>
#include <limits>

namespace UpdateChecker
{
namespace
{

// Reads a core component at the start of rest, a decimal number without a leading zero that fits in
// 64 bits, and removes it from rest.
std::optional<std::uint64_t> takeSemverComponent(std::string_view &rest)
{
    std::size_t length = 0;
    std::uint64_t component = 0;
    while (length < rest.size() && rest[length] >= '0' && rest[length] <= '9')
    {
        const auto digit = static_cast<std::uint64_t>(rest[length] - '0');
        if (component > (std::numeric_limits<std::uint64_t>::max() - digit) / 10)
        {
            return std::nullopt;
        }
        component = component * 10 + digit;
        ++length;
    }
    if (length == 0 || (length > 1 && rest.front() == '0'))
    {
        return std::nullopt;
    }
    rest.remove_prefix(length);
    return component;
}

} // namespace

// The suffix is not checked, except that it must not contain a line break.
std::optional<SemverCore> parseSemverCore(std::string_view version)
{
    SemverCore core{};
    for (std::size_t index = 0; index < core.size(); ++index)
    {
        if (index > 0)
        {
            if (version.empty() || version.front() != '.')
            {
                return std::nullopt;
            }
            version.remove_prefix(1);
        }
        const auto component = takeSemverComponent(version);
        if (!component)
        {
            return std::nullopt;
        }
        core[index] = *component;
    }

    if (!version.empty() && version.front() != '-' && version.front() != '+')
    {
        return std::nullopt;
    }
    if (version.find_first_of("\r\n") != std::string_view::npos)
    {
        return std::nullopt;
    }
    return core;
}

bool isSemverNewer(std::string_view candidate, std::string_view current)
{
    const auto candidateCore = parseSemverCore(candidate);
    const auto currentCore = parseSemverCore(current);
    return candidateCore && currentCore && *candidateCore > *currentCore;
}

} // namespace UpdateChecker
