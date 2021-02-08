/**
 * This file test the `error` module
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: error_test.cpp
 * @author: Steven R. Emmerson
 */

#include "error.h"

#include <exception>
#include <gtest/gtest.h>
#include <thread>

namespace {

// The fixture for testing module `error`.
class ErrorTest : public ::testing::Test {
protected:
};

// Tests simple logging
TEST_F(ErrorTest, SimpleLogging) {
    hycast::logThreshold = hycast::LogLevel::DEBUG;
    LOG_DEBUG("Debug message");
    LOG_INFO("Info message");
    LOG_NOTE("Notice message");
    LOG_WARN("Warning message");
    LOG_ERROR("Error message");
    LOG_DEBUG("Debug message %d", 1);
    LOG_INFO("Info message %d", 2);
    LOG_NOTE("Notice message %d", 3);
    LOG_WARN("Warning message %d", 4);
    LOG_ERROR("Error message %d", 5);
}

// Tests exception logging levels
TEST_F(ErrorTest, ExceptionLoggingLevels) {
    hycast::logThreshold = hycast::LogLevel::DEBUG;
    hycast::log_error(hycast::RUNTIME_ERROR("Error level"));
    hycast::log_warn(hycast::RUNTIME_ERROR("Warning level"));
    hycast::log_note(hycast::RUNTIME_ERROR("Notice level"));
    hycast::log_info(hycast::RUNTIME_ERROR("Informational level"));
    hycast::log_debug(hycast::RUNTIME_ERROR("Debug level"));
}

// Tests system error
TEST_F(ErrorTest, SystemError) {
    try {
        errno = 1;
        throw hycast::SYSTEM_ERROR("System error");
    }
    catch (const std::exception& e) {
        hycast::log_error(e);
    }
    try {
        throw hycast::SYSTEM_ERROR("System error", 2);
    }
    catch (const std::exception& e) {
        hycast::log_error(e);
    }
    try {
        errno = 1;
        throw hycast::SYSTEM_ERROR("System error");
    }
    catch (const std::exception& e) {
        hycast::log_error(e);
    }
    try {
        throw hycast::SYSTEM_ERROR("System error", 2);
    }
    catch (const std::exception& e) {
        hycast::log_error(e);
    }
}


static void throwNestedException() {
    try {
        try {
            errno = 1;
            throw hycast::SYSTEM_ERROR("Inner exception");
        }
        catch (const std::exception& e) {
            std::throw_with_nested(hycast::RUNTIME_ERROR("Outer exception"));
        }
    }
    catch (const std::exception& e) {
        hycast::log_error(e);
    }
}

// Tests nested exception
TEST_F(ErrorTest, NestedException) {
    throwNestedException();
}

#if 0
// Tests nested exception in separate thread
TEST_F(ErrorTest, NestedExceptionInThread) {
    std::thread thread{throwNestedException};
    thread.join();
}
#endif

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
