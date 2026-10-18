// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <doctest.h>
#include <iostream>

namespace TestHelpers {
inline unsigned executedTestCases = 0;

struct ExecutionListener final : doctest::IReporter {
	explicit ExecutionListener(const doctest::ContextOptions &) {}
	void report_query(const doctest::QueryData &) override {}
	void test_run_start() override { executedTestCases = 0; }
	void test_run_end(const doctest::TestRunStats &) override {}
	void test_case_start(const doctest::TestCaseData &) override { ++executedTestCases; }
	void test_case_reenter(const doctest::TestCaseData &) override {}
	void test_case_end(const doctest::CurrentTestCaseStats &) override {}
	void test_case_exception(const doctest::TestCaseException &) override {}
	void subcase_start(const doctest::SubcaseSignature &) override {}
	void subcase_end() override {}
	void log_assert(const doctest::AssertData &) override {}
	void log_message(const doctest::MessageData &) override {}
	void test_case_skipped(const doctest::TestCaseData &) override {}
};

inline int runTests(doctest::Context &context)
{
	const int result = context.run();
	if (!context.shouldExit() && executedTestCases == 0) {
		std::cerr << "No test cases were executed\n";
		return 1;
	}
	return result;
}
} // namespace TestHelpers

DOCTEST_REGISTER_LISTENER("require-executed-tests", 0, TestHelpers::ExecutionListener);
