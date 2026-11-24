// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>

#include <DoctestRunner.hpp>

#include <iostream>
#include <string>

#include "ObsHarness.hpp"

int main(int argc, char **argv)
{
    doctest::Context context(argc, argv);

    std::string error;
    if (!ObsHarness::start(error))
    {
        std::cerr << "Failed to start libobs: " << error << '\n';
        ObsHarness::stop();
        return 1;
    }
    const int result = TestHelpers::runTests(context);
    ObsHarness::stop();
    return result;
}
