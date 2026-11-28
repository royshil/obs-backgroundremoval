// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>

#include <DoctestRunner.hpp>

int main(int argc, char **argv)
{
    doctest::Context context(argc, argv);
    return TestHelpers::runTests(context);
}
