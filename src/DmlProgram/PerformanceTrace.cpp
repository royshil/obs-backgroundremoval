// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
// SPDX-License-Identifier: Apache-2.0
#include "PerformanceTrace.hpp"
#include <windows.h>
#include <winmeta.h>
#include <TraceLoggingProvider.h>
#include <atomic>
#include <exception>

namespace BaselineDml::PerformanceTrace {
namespace {
TRACELOGGING_DEFINE_PROVIDER(provider, "ObsBackgroundRemoval.DirectML",
			     (0x50ec32f6, 0xbd87, 0x4b42, 0x93, 0x0c, 0x89, 0xd9, 0xd7, 0xed, 0x42, 0xb3));
class Registration {
public:
	Registration() noexcept { TraceLoggingRegister(provider); }
	~Registration() noexcept { TraceLoggingUnregister(provider); }
};
bool enabled() noexcept
{
	static Registration registration;
	return TraceLoggingProviderEnabled(provider, WINEVENT_LEVEL_VERBOSE, 0x1);
}
std::atomic<uint64_t> nextId{0};
} // namespace
Scope::Scope(const char *name) noexcept : name_(name), enabled_(enabled())
{
	if (!enabled_)
		return;
	id_ = ++nextId;
	exceptions_ = std::uncaught_exceptions();
	start_ = std::chrono::steady_clock::now();
	TraceLoggingWrite(provider, "ScopeStart", TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE), TraceLoggingKeyword(0x1),
			  TraceLoggingString(name_, "Scope"), TraceLoggingUInt64(id_, "ScopeId"));
}
Scope::~Scope() noexcept
{
	finish();
}
void Scope::finish() noexcept
{
	if (!enabled_)
		return;
	const double duration =
		std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start_).count();
	TraceLoggingWrite(provider, "ScopeStop", TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE), TraceLoggingKeyword(0x1),
			  TraceLoggingString(name_, "Scope"), TraceLoggingUInt64(id_, "ScopeId"),
			  TraceLoggingFloat64(duration, "DurationMs"),
			  TraceLoggingBool(std::uncaught_exceptions() > exceptions_, "ExceptionInFlight"));
	enabled_ = false;
}
void inferenceDecision(bool required) noexcept
{
	if (!enabled())
		return;
	TraceLoggingWrite(provider, "InferenceDecision", TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
			  TraceLoggingKeyword(0x1), TraceLoggingBool(required, "Required"));
}
} // namespace BaselineDml::PerformanceTrace
