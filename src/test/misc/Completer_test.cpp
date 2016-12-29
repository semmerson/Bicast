/**
 * This file tests the class `Completer`.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Completer_test.cpp
 * @author: Steven R. Emmerson
 */


#include "Completer.h"

#include <array>
#include <gtest/gtest.h>

namespace {

// The fixture for testing class Completer.
class CompleterTest : public ::testing::Test {
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    CompleterTest() {
        // You can do set-up work for each test here.
    }

    virtual ~CompleterTest() {
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

    // Objects declared here can be used by all tests in the test case for Completer.
};

// Tests construction of void completer
TEST_F(CompleterTest, VoidConstruction) {
    hycast::Completer<void> completer{};
}

// Tests construction of int completer
TEST_F(CompleterTest, IntConstruction) {
    hycast::Completer<int> completer{};
}

// Tests execution of void task
TEST_F(CompleterTest, VoidExecution) {
    hycast::Completer<void> completer{};
    auto future1 = completer.submit([]{return;});
    auto future2 = completer.get();
    EXPECT_TRUE(future1 == future2);
    future2.getResult();
    EXPECT_FALSE(future2.wasCancelled());
}

// Tests execution of int task
TEST_F(CompleterTest, IntExecution) {
    hycast::Completer<int> completer{};
    auto future1 = completer.submit([]{return 1;});
    auto future2 = completer.get();
    EXPECT_TRUE(future1 == future2);
    EXPECT_EQ(1, future2.getResult());
    EXPECT_FALSE(future2.wasCancelled());
}

// Tests execution of multiple void tasks
TEST_F(CompleterTest, MultipleVoidExecution) {
    hycast::Completer<void> completer{};
    std::array<hycast::Future<void>, 8> futures;
    for (unsigned i = 0; i < futures.size(); ++i) {
        auto future = completer.submit([]{return;});
        futures[i] = future;
    }
    for (unsigned i = 0; i < futures.size(); ++i) {
        auto future = completer.get();
        future.getResult();
        EXPECT_FALSE(future.wasCancelled());
    }
}

// Tests execution of multiple int tasks
TEST_F(CompleterTest, MultipleIntExecution) {
    hycast::Completer<int> completer{};
    std::array<hycast::Future<int>, 8> futures;
    for (unsigned i = 0; i < futures.size(); ++i) {
        auto future = completer.submit([i]{return i;});
        futures[i] = future;
    }
    for (unsigned i = 0; i < futures.size(); ++i) {
        auto future = completer.get();
        int j = future.getResult();
        EXPECT_FALSE(future.wasCancelled());
        EXPECT_TRUE(futures[j] == future);
    }
}

// Tests cancellation of void task
TEST_F(CompleterTest, VoidCancellation) {
    hycast::Completer<void> completer{};
    auto future = completer.submit([]{::pause();});
    future.cancel();
    EXPECT_TRUE(future.wasCancelled());
    EXPECT_THROW(future.getResult(), std::logic_error);
}

// Tests cancellation of int task
TEST_F(CompleterTest, IntCancellation) {
    hycast::Completer<int> completer{};
    auto future = completer.submit([]{::pause(); return 1;});
    future.cancel();
    EXPECT_TRUE(future.wasCancelled());
    EXPECT_THROW(future.getResult(), std::logic_error);
}

// Tests shutting-down void completer
TEST_F(CompleterTest, VoidShutdown) {
    hycast::Completer<void> completer{};
    auto future = completer.submit([]{::pause();});
    completer.shutdownNow();
    EXPECT_THROW(completer.submit([]{return;}), std::logic_error);
    completer.awaitTermination();
    EXPECT_TRUE(future.wasCancelled());
}

// Tests shutting-down int completer
TEST_F(CompleterTest, IntShutdown) {
    hycast::Completer<int> completer{};
    auto future = completer.submit([]{::pause(); return 1;});
    completer.shutdownNow();
    EXPECT_THROW(completer.submit([]{return 2;}), std::logic_error);
    completer.awaitTermination();
    EXPECT_TRUE(future.wasCancelled());
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
