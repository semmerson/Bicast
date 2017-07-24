/**
 * This file tests class `Future`.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Future_test.cpp
 * @author: Steven R. Emmerson
 */

#include "error.h"
#include "Future.h"
#include "Task.h"
#include "Thread.h"

#include <atomic>
#include <condition_variable>
#include <exception>
#include <gtest/gtest.h>
#include <iostream>
#include <mutex>
#include <pthread.h>
#include <random>
#include <stdexcept>
#include <stdio.h>
#include <thread>
#include <unistd.h>

namespace {

// The fixture for testing class Future.
class FutureTest : public ::testing::Test
{};

#if 1
// Tests execution of void task
TEST_F(FutureTest, VoidTaskExecution)
{
    hycast::Task<void> task{[]{}};
    EXPECT_TRUE(task);
    hycast::Future<void> future{task};
    EXPECT_TRUE(future);

    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread(future);
    EXPECT_FALSE(future.wasCanceled());
    EXPECT_NO_THROW(future.getResult());
}

// Tests relational operators between void tasks
TEST_F(FutureTest, VoidTaskRelationalOperations)
{
    hycast::Task<void> task1{[]{}};
    EXPECT_TRUE(task1);
    hycast::Future<void> future1{task1};
    EXPECT_TRUE(future1);
    EXPECT_TRUE(future1 == future1);

    hycast::Task<void> task2{[]{}};
    EXPECT_TRUE(task2);
    hycast::Future<void> future2{task2};
    EXPECT_TRUE(future2);
    EXPECT_TRUE(future1 != future2);
    EXPECT_TRUE(future1 < future2);
}

// Tests void task cancellation
TEST_F(FutureTest, VoidTaskCancellation)
{
    hycast::Task<void> task{[]{::pause();}};
    hycast::Future<void> future{task};
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread(future);
    ASSERT_EQ(1, hycast::Thread::size());
    future.cancel();
    EXPECT_TRUE(future.wasCanceled());
    thread.join();
    EXPECT_EQ(0, hycast::Thread::size());
    EXPECT_THROW(future.getResult(), hycast::LogicError);
}
#endif

// Tests void task thread-cancellation
TEST_F(FutureTest, VoidTaskThreadCancellation)
{
    hycast::Task<void> task{::pause};
    hycast::Future<void> future{task};
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{future};
    ASSERT_EQ(1, hycast::Thread::size());
    thread.cancel();
    EXPECT_TRUE(future.wasCanceled());
    thread.join();
    EXPECT_EQ(0, hycast::Thread::size());
    EXPECT_THROW(future.getResult(), hycast::LogicError);
}

#if 1
// Tests void task cancellation loop
TEST_F(FutureTest, VoidTaskCancellationLoop)
{
    std::set_terminate([]{::pause();});
    for (int i = 0; i < 1000; ++i) {
        std::cout << i << '\n';
        hycast::Task<void> task{[]{::pause();}};
        hycast::Future<void> future{task};
        ASSERT_EQ(0, hycast::Thread::size());
        hycast::Thread thread{future};
        ASSERT_EQ(1, hycast::Thread::size());
        future.cancel();
        thread.join();
        EXPECT_TRUE(future.wasCanceled());
        EXPECT_EQ(0, hycast::Thread::size());
    }
}

// Tests getting result of void task
TEST_F(FutureTest, VoidTaskResult)
{
    hycast::Task<void> task{[]{}};
    hycast::Future<void> future{task};
    hycast::Thread thread(future);
    EXPECT_NO_THROW(future.getResult());
    EXPECT_FALSE(future.wasCanceled());
}

// Tests waiting on void task
TEST_F(FutureTest, VoidTaskWaiting)
{
    hycast::Task<void> task{[]{}};
    hycast::Future<void> future{task};
    hycast::Thread thread(future);
    EXPECT_FALSE(future.wasCanceled());
    EXPECT_NO_THROW(future.getResult());
}

// Tests void future cancellation ordering
TEST_F(FutureTest, VoidFutureCancelOrdering)
{
    hycast::Task<void> task{[]{}};
    hycast::Future<void> future{task};
    hycast::Thread thread(future);
    EXPECT_FALSE(future.wasCanceled());
    future.cancel();
    thread.join();
    EXPECT_FALSE(future.wasCanceled());
    EXPECT_NO_THROW(future.getResult());
}

// Tests void future exception ordering
TEST_F(FutureTest, VoidFutureExceptionOrdering)
{
    hycast::Task<void> task{[]{throw hycast::RuntimeError(__FILE__, __LINE__,
            "Hi there!");}};
    hycast::Future<void> future{task};
    hycast::Thread thread(future);
    EXPECT_FALSE(future.wasCanceled());
    future.cancel();
    thread.join();
    EXPECT_FALSE(future.wasCanceled());
    EXPECT_THROW(future.getResult(), hycast::RuntimeError);
}

/******************************************************************************/

// Tests execution of int task
TEST_F(FutureTest, IntTaskExecution)
{
    hycast::Task<int> task{[]{return 1;}};
    EXPECT_TRUE(task);
    hycast::Future<int> future{task};
    EXPECT_TRUE(future);

    hycast::Thread thread(future);
    EXPECT_FALSE(future.wasCanceled());
    EXPECT_EQ(1, future.getResult());
}

// Tests relational operators between int tasks
TEST_F(FutureTest, IntTaskRelationalOperations)
{
    hycast::Task<int> task1{[]{return 1;}};
    EXPECT_TRUE(task1);
    hycast::Future<int> future1{task1};
    EXPECT_TRUE(future1);
    EXPECT_TRUE(future1 == future1);

    hycast::Task<int> task2{[]{return 2;}};
    EXPECT_TRUE(task2);
    hycast::Future<int> future2{task2};
    EXPECT_TRUE(future2);
    EXPECT_TRUE(future1 != future2);
}

// Tests int task cancellation
TEST_F(FutureTest, IntTaskCancellation)
{
    hycast::Task<int> task{[]{::pause(); return 1;}};
    hycast::Future<int> future{task};
    hycast::Thread thread(future);
    future.cancel();
    thread.join();
    EXPECT_TRUE(future.wasCanceled());
    EXPECT_THROW(future.getResult(), hycast::LogicError);
}

// Tests getting result of int task
TEST_F(FutureTest, IntTaskResult)
{
    hycast::Task<int> task{[]{return 0;}};
    hycast::Future<int> future{task};
    hycast::Thread thread(future);
    EXPECT_FALSE(future.wasCanceled());
    EXPECT_NO_THROW(future.getResult());
}

// Tests waiting on int task
TEST_F(FutureTest, IntTaskWaiting)
{
    hycast::Task<int> task{[]{return 1;}};
    hycast::Future<int> future{task};
    hycast::Thread thread(future);
    EXPECT_FALSE(future.wasCanceled());
    EXPECT_EQ(1, future.getResult());
}

// Tests
TEST_F(FutureTest, IntFutureCancelOrdering)
{
    hycast::Task<int> task{[]{return 1;}};
    hycast::Future<int> future{task};
    hycast::Thread thread(future);
    EXPECT_FALSE(future.wasCanceled());
    future.cancel();
    thread.join();
    EXPECT_FALSE(future.wasCanceled());
    EXPECT_NO_THROW(future.getResult());
}

// Tests
TEST_F(FutureTest, IntFutureExceptionOrdering)
{
    hycast::Task<int> task{[]{throw hycast::RuntimeError(__FILE__, __LINE__,
            "Hi there!"); return 1;}};
    hycast::Future<int> future{task};
    hycast::Thread thread(future);
    EXPECT_FALSE(future.wasCanceled());
    thread.join();
    EXPECT_FALSE(future.wasCanceled());
    EXPECT_THROW(future.getResult(), hycast::RuntimeError);
}

// Tests execution of a bunch of futures
TEST_F(FutureTest, BunchOfFutures) {
    std::default_random_engine generator{};
    std::uniform_int_distribution<useconds_t> distribution{0, 10000};
    hycast::Thread threads[200];
    std::mutex mutex;
    std::unique_lock<decltype(mutex)> lock{mutex};
    std::condition_variable cond{};
    int count{0};
    for (int i = 0; i < 200; ++i) {
        hycast::Task<void> task{[&generator,&distribution,&mutex,&count,&cond]()
                mutable {
            ::usleep(distribution(generator));
            std::lock_guard<decltype(mutex)> lock{mutex};
            ++count;
            cond.notify_one();
        }};
        auto future = hycast::Future<void>{task};
        threads[i] = hycast::Thread{future};
    }
    while (count < 100)
        cond.wait(lock);
}

#endif
}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
