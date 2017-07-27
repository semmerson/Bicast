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

#include <chrono>
#include <exception>
#include <gtest/gtest.h>
#include <iostream>
#include <random>
#include <stdexcept>
#include <unistd.h>

namespace {

// The fixture for testing class Executor.
class ExecutorTest : public ::testing::Test {
};

// Tests default construction of void executor
TEST_F(ExecutorTest, DefaultVoidConstruction) {
    hycast::Executor<void> executor{};
}

// Tests default construction of int executor
TEST_F(ExecutorTest, DefaultIntConstruction) {
    hycast::Executor<int> executor{};
}

// Tests executing void task
TEST_F(ExecutorTest, VoidExecution) {
    hycast::Executor<void> executor{};
    auto future = executor.submit([]{});
    EXPECT_FALSE(future.wasCanceled());
    future.getResult();
}

// Tests executing int task
TEST_F(ExecutorTest, IntExecution) {
    hycast::Executor<int> executor{};
    auto future = executor.submit([]{return 1;});
    EXPECT_FALSE(future.wasCanceled());
    EXPECT_EQ(1, future.getResult());
}

// Tests canceling void task
TEST_F(ExecutorTest, CancelVoid) {
    hycast::Executor<void> executor{};
    auto future = executor.submit([]{::pause();});
    future.cancel();
    EXPECT_TRUE(future.wasCanceled());
    EXPECT_THROW(future.getResult(), std::logic_error);
}

// Tests canceling int task
TEST_F(ExecutorTest, CancelInt) {
    hycast::Executor<int> executor{};
    auto future = executor.submit([]{::pause(); return 1;});
    future.cancel();
    EXPECT_TRUE(future.wasCanceled());
    EXPECT_THROW(future.getResult(), std::logic_error);
}

// Tests soft shutdown of void executor
TEST_F(ExecutorTest, SoftVoidShutdown) {
    hycast::Executor<void> executor{};
    auto future = executor.submit([]{::usleep(100000);});
    executor.shutdown(false);
    executor.awaitTermination();
    EXPECT_FALSE(future.wasCanceled());
    EXPECT_NO_THROW(future.getResult());
}

// Tests hard shutdown of void executor
TEST_F(ExecutorTest, HardVoidShutdown) {
    hycast::Executor<void> executor{};
    auto future = executor.submit([]{::pause();});
    executor.shutdown();
    executor.awaitTermination();
    EXPECT_TRUE(future.wasCanceled());
    EXPECT_THROW(future.getResult(), hycast::LogicError);
}

// Tests destruction with active task
TEST_F(ExecutorTest, DestructionWithTask) {
    hycast::Future<void> future{};
    {
        hycast::Executor<void> executor{};
        future = executor.submit([]{::pause();});
    }
    EXPECT_TRUE(future.wasCanceled());
    EXPECT_THROW(future.getResult(), hycast::LogicError);
}

// Tests exception in void task
TEST_F(ExecutorTest, VoidException) {
    hycast::Executor<void> executor{};
    auto future = executor.submit([]{throw std::runtime_error("Dummy");});
    EXPECT_FALSE(future.wasCanceled());
    //future.getResult();
    EXPECT_THROW(future.getResult(), std::runtime_error);
}

#if 0
// Tests exception in int task
TEST_F(ExecutorTest, IntException) {
    hycast::Executor<int> executor{};
    auto future = executor.submit([]{throw std::runtime_error("Dummy"); return 1;});
    EXPECT_FALSE(future.wasCanceled());
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

// Tests execution of a bunch of tasks
TEST_F(ExecutorTest, BunchOfJobs) {
    hycast::Executor<void> executor{};
    std::default_random_engine generator{};
    std::uniform_int_distribution<useconds_t> distribution{0, 100000};
    for (int i = 0; i < 200; ++i)
        executor.submit([&generator,&distribution]() mutable
                {::usleep(distribution(generator));});
    executor.shutdown(false);
    executor.awaitTermination();
}

// Tests construction and cancellation performance
TEST_F(ExecutorTest, CtorAndCancelPerformance) {
    std::set_terminate([]{::pause();}); // For debugging
    typedef std::chrono::microseconds      TimeUnit;
    typedef std::chrono::steady_clock      Clock;
    typedef std::chrono::time_point<Clock> TimePoint;

    const TimePoint start = Clock::now();
    const int       numExec = 1000;
    int             i;

    try {
        for (i = 0; i < numExec; ++i) {
            hycast::Executor<void> executor{};
            auto future = executor.submit([]{::pause();});
            future.cancel();
            EXPECT_TRUE(future.wasCanceled());
        }

        TimePoint stop = Clock::now();
        TimeUnit  duration = std::chrono::duration_cast<TimeUnit>(stop - start);
        std::cout << numExec << " executions in " << duration.count() <<
                " microseconds = " << 1000000*numExec/duration.count() <<
                " Hz\n";
    }
    catch (const std::exception& e) {
        hycast::log_what(e, __FILE__, __LINE__, "Failed on iteration " +
                std::to_string(i));
    }
}
#endif

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
