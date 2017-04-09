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

namespace {

// The fixture for testing module `error`.
class ErrorTest : public ::testing::Test {
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    ErrorTest() {
        // You can do set-up work for each test here.
    }

    virtual ~ErrorTest() {
        // You can do clean-up work that doesn't throw exceptions here.
    }

    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:

    virtual void SetUp() {
        // Code here will be called immediately after the constructor (right
        // before each test).
    }

    virtual void TearDown() {
        // Code here will be called immediately after each test (right
        // before the destructor).
    }

    // Objects declared here can be used by all tests in the test case for Error.
};

// Tests construction
TEST_F(ErrorTest, SystemError) {
    try {
        errno = 1;
        throw hycast::SystemError(__FILE__, __LINE__, "This is a test");
    }
    catch (const std::exception& e) {
        hycast::log_what(e);
    }
    EXPECT_EQ(0, 0);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
