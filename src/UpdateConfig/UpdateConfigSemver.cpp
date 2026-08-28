// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "UpdateConfig.hpp"

#include <cstddef>
#include <limits>
#include <regex>
#include <stdexcept>
#include <tuple>

namespace UpdateConfig {
namespace {

std::optional<std::uint64_t> matchSemverComponent(std::string_view version, std::size_t componentIndex)
{
	static const std::regex pattern{R"(^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:[-+].*)?$)"};
	std::match_results<std::string_view::const_iterator> match;
	if (!std::regex_match(version.begin(), version.end(), match, pattern)) {
		return std::nullopt;
	}

	try {
		const auto component = std::stoull(match[componentIndex].str());
		if (component > std::numeric_limits<std::uint64_t>::max()) {
			return std::nullopt;
		}
		return static_cast<std::uint64_t>(component);
	} catch (const std::invalid_argument &) {
		return std::nullopt;
	} catch (const std::out_of_range &) {
		return std::nullopt;
	}
}

} // namespace

std::optional<std::uint64_t> getSemverMajor(std::string_view version)
{
	return matchSemverComponent(version, 1);
}

std::optional<std::uint64_t> getSemverMinor(std::string_view version)
{
	return matchSemverComponent(version, 2);
}

std::optional<std::uint64_t> getSemverPatch(std::string_view version)
{
	return matchSemverComponent(version, 3);
}

bool isSemverNewer(std::string_view candidate, std::string_view current)
{
	const auto candidateMajor = getSemverMajor(candidate);
	const auto candidateMinor = getSemverMinor(candidate);
	const auto candidatePatch = getSemverPatch(candidate);
	const auto currentMajor = getSemverMajor(current);
	const auto currentMinor = getSemverMinor(current);
	const auto currentPatch = getSemverPatch(current);
	if (!candidateMajor || !candidateMinor || !candidatePatch || !currentMajor || !currentMinor || !currentPatch) {
		return false;
	}

	return std::tie(*candidateMajor, *candidateMinor, *candidatePatch) >
	       std::tie(*currentMajor, *currentMinor, *currentPatch);
}

} // namespace UpdateConfig
