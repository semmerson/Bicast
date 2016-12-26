/**
 * This file tests the class `Executor`.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Executor_test.cpp
 * @author: Steven R. Emmerson
 */

#include "Executor.h"

#include <gtest/gtest.h>

namespace {

// The fixture for testing class Executor.
class ExecutorTest : public ::testing::Test {
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    ExecutorTest() {
        // You can do set-up work for each test here.
    }

    virtual ~ExecutorTest() {
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

    // Objects declared here can be used by all tests in the test case for Executor.
};

// Tests default construction of void executor
TEST_F(ExecutorTest, DefaultVoidConstruction) {
    hycast::Executor<void> executor{};
}

// Tests default construction of int executor
TEST_F(ExecutorTest, DefaultIntConstruction) {
    hycast::Executor<int> executor{};
}

// Tests waiting on future of void task
TEST_F(ExecutorTest, DefaultVoidExecution) {
    hycast::Executor<void> executor{};
    hycast::Future<void> future = executor.submit([]{return;});
    future.wait();
    EXPECT_FALSE(future.wasCancelled());
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
