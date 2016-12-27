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
#include <stdexcept>

namespace {

// The fixture for testing class Executor.
class ExecutorTest : public ::testing::Test {
};

// Tests construction of void executor
TEST_F(ExecutorTest, DefaultVoidConstruction) {
    hycast::Executor<void> executor{};
}

// Tests construction of int executor
TEST_F(ExecutorTest, IntConstruction) {
    hycast::Executor<int> executor{};
}

// Tests executing void task
TEST_F(ExecutorTest, VoidExecution) {
    hycast::Executor<void> executor{};
    auto future = executor.submit([]{return;});
    future.wait();
    EXPECT_FALSE(future.wasCancelled());
    future.getResult();
}

// Tests executing int task
TEST_F(ExecutorTest, IntExecution) {
    hycast::Executor<int> executor{};
    auto future = executor.submit([]{return 1;});
    future.wait();
    EXPECT_FALSE(future.wasCancelled());
    EXPECT_EQ(1, future.getResult());
}

// Tests canceling void task
TEST_F(ExecutorTest, CancelVoid) {
    hycast::Executor<void> executor{};
    auto future = executor.submit([]{::pause();});
    future.cancel();
    EXPECT_TRUE(future.wasCancelled());
    EXPECT_THROW(future.getResult(), std::logic_error);
}

// Tests canceling int task
TEST_F(ExecutorTest, CancelInt) {
    hycast::Executor<int> executor{};
    auto future = executor.submit([]{::pause(); return 1;});
    future.cancel();
    EXPECT_TRUE(future.wasCancelled());
    EXPECT_THROW(future.getResult(), std::logic_error);
}

// Tests exception in void task
TEST_F(ExecutorTest, VoidException) {
    hycast::Executor<void> executor{};
    auto future = executor.submit([]{throw std::runtime_error("Dummy");});
    future.wait();
    EXPECT_FALSE(future.wasCancelled());
    EXPECT_THROW(future.getResult(), std::runtime_error);
}

// Tests exception in int task
TEST_F(ExecutorTest, IntException) {
    hycast::Executor<int> executor{};
    auto future = executor.submit([]{throw std::runtime_error("Dummy"); return 1;});
    future.wait();
    EXPECT_FALSE(future.wasCancelled());
    EXPECT_THROW(future.getResult(), std::runtime_error);
}

// Tests equality operator of void task
TEST_F(ExecutorTest, VoidEquality) {
    hycast::Executor<void> executor{};
    auto future1 = executor.submit([]{return;});
    EXPECT_TRUE(future1 == future1);
    auto future2 = executor.submit([]{return;});
    EXPECT_FALSE(future1 == future2);
    future1.getResult();
    future2.getResult();
}

// Tests equality operator of int task
TEST_F(ExecutorTest, IntEquality) {
    hycast::Executor<int> executor{};
    auto future1 = executor.submit([]{return 1;});
    EXPECT_TRUE(future1 == future1);
    auto future2 = executor.submit([]{return 1;});
    EXPECT_FALSE(future1 == future2);
    future1.getResult();
    future2.getResult();
}

// Tests less-than operator of void task
TEST_F(ExecutorTest, CompareVoid) {
    hycast::Executor<void> executor{};
    auto future1 = executor.submit([]{return;});
    EXPECT_FALSE(future1 < future1);
    auto future2 = executor.submit([]{return;});
    EXPECT_TRUE(future1 < future2 || future2 < future1);
    EXPECT_FALSE(future1 < future2 && future2 < future1);
    future1.getResult();
    future2.getResult();
}

// Tests less-than operator of int task
TEST_F(ExecutorTest, CompareInt) {
    hycast::Executor<int> executor{};
    auto future1 = executor.submit([]{return 1;});
    EXPECT_FALSE(future1 < future1);
    auto future2 = executor.submit([]{return 1;});
    EXPECT_TRUE(future1 < future2 || future2 < future1);
    EXPECT_FALSE(future1 < future2 && future2 < future1);
    future1.getResult();
    future2.getResult();
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
