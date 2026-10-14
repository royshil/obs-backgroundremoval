// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chrono>
#include <cstdint>

namespace BaselineDml::PerformanceTrace {
class Scope final {
public:
	explicit Scope(const char *name) noexcept;
	~Scope() noexcept;
	void finish() noexcept;
	Scope(const Scope &) = delete;
	Scope &operator=(const Scope &) = delete;

private:
	const char *name_;
	uint64_t id_ = 0;
	int exceptions_ = 0;
	bool enabled_ = false;
	std::chrono::steady_clock::time_point start_;
};
void inferenceDecision(bool required) noexcept;
} // namespace BaselineDml::PerformanceTrace
