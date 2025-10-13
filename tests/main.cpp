//  Copyright Marcos Cambón-López 2025.

// Distributed under the Mozilla Public License Version 2.0.
//    (See accompanying file LICENSE or copy at
//          https://www.mozilla.org/en-US/MPL/2.0/)

#include <gtest/gtest.h> // googletest header file
#include <spdlog/spdlog.h>
#include "spdlog/sinks/stdout_color_sinks.h" 

#define INIT_LOGGER(name, pattern) spdlog::set_pattern("[%H:%M:%S] [%^%t%$] [%^%l%$] %v"); \
                                   auto name = spdlog::stdout_color_mt(#name)


int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    INIT_LOGGER(logger, "[%H:%M:%S] [%^%t%$] [%^%l%$] %v"); // Initialize logger with custom pattern

    spdlog::set_level(spdlog::level::debug); 

    return RUN_ALL_TESTS();
}