/**
 * This file tests the classes `Task` and `Future`.
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

#include <exception>
#include <gtest/gtest.h>
#include <iostream>
#include <pthread.h>
#include <stdexcept>
#include <stdio.h>
#include <thread>
#include <unistd.h>

namespace {

// The fixture for testing classes Task and Future.
class TaskFutureTest : public ::testing::Test
{};

// Tests default constructed void task
TEST_F(TaskFutureTest, DefaultVoidTaskConstruction)
{
    hycast::Task<void> task{};
    EXPECT_FALSE(task);
}

// Tests execution of void task
TEST_F(TaskFutureTest, VoidTaskExecution)
{
    hycast::Task<void> task{[]{}};
    EXPECT_TRUE(task);
    hycast::Future<void> future{task};
    EXPECT_TRUE(future);

    hycast::Thread thread(future);
    EXPECT_FALSE(future.wasCanceled());
    EXPECT_NO_THROW(future.getResult());
}

// Tests relational operators between void tasks
TEST_F(TaskFutureTest, VoidTaskRelationalOperations)
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
TEST_F(TaskFutureTest, VoidTaskCancellation)
{
    hycast::Task<void> task{[]{::pause();}};
    hycast::Future<void> future{task};
    hycast::Thread thread(future);
    future.cancel();
    EXPECT_TRUE(future.wasCanceled());
    EXPECT_THROW(future.getResult(), hycast::LogicError);
}

// Tests void task cancellation loop
TEST_F(TaskFutureTest, VoidTaskCancellationLoop)
{
    std::set_terminate([]{::pause();});
    for (int i = 0; i < 1000; ++i) {
        std::cout << i << '\n';
        hycast::Task<void> task{[]{::pause();}};
        hycast::Future<void> future{task};
        hycast::Thread thread(future);
        future.cancel();
        EXPECT_TRUE(future.wasCanceled());
    }
}

// Tests getting result of void task
TEST_F(TaskFutureTest, VoidTaskResult)
{
    hycast::Task<void> task{[]{}};
    hycast::Future<void> future{task};
    hycast::Thread thread(future);
    EXPECT_NO_THROW(future.getResult());
    EXPECT_FALSE(future.wasCanceled());
}

// Tests waiting on void task
TEST_F(TaskFutureTest, VoidTaskWaiting)
{
    hycast::Task<void> task{[]{}};
    hycast::Future<void> future{task};
    hycast::Thread thread(future);
    EXPECT_FALSE(future.wasCanceled());
    EXPECT_NO_THROW(future.getResult());
}

// Tests void future cancellation ordering
TEST_F(TaskFutureTest, VoidFutureCancelOrdering)
{
    hycast::Task<void> task{[]{}};
    hycast::Future<void> future{task};
    hycast::Thread thread(future);
    EXPECT_FALSE(future.wasCanceled());
    future.cancel();
    EXPECT_FALSE(future.wasCanceled());
    EXPECT_NO_THROW(future.getResult());
}

// Tests void future exception ordering
TEST_F(TaskFutureTest, VoidFutureExceptionOrdering)
{
    hycast::Task<void> task{[]{throw hycast::RuntimeError(__FILE__, __LINE__,
            "Hi there!");}};
    hycast::Future<void> future{task};
    hycast::Thread thread(future);
    EXPECT_FALSE(future.wasCanceled());
    future.cancel();
    EXPECT_FALSE(future.wasCanceled());
    EXPECT_THROW(future.getResult(), hycast::RuntimeError);
}

/******************************************************************************/

// Tests default constructed int task
TEST_F(TaskFutureTest, DefaultIntTaskConstruction)
{
    hycast::Task<int> task{};
    EXPECT_FALSE(task);
}

// Tests execution of int task
TEST_F(TaskFutureTest, IntTaskExecution)
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
TEST_F(TaskFutureTest, IntTaskRelationalOperations)
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
TEST_F(TaskFutureTest, IntTaskCancellation)
{
    hycast::Task<int> task{[]{::pause(); return 1;}};
    hycast::Future<int> future{task};
    hycast::Thread thread(future);
    future.cancel();
    EXPECT_TRUE(future.wasCanceled());
    EXPECT_THROW(future.getResult(), hycast::LogicError);
}

// Tests getting result of int task
TEST_F(TaskFutureTest, IntTaskResult)
{
    hycast::Task<int> task{[]{return 0;}};
    hycast::Future<int> future{task};
    hycast::Thread thread(future);
    EXPECT_FALSE(future.wasCanceled());
    EXPECT_NO_THROW(future.getResult());
}

// Tests waiting on int task
TEST_F(TaskFutureTest, IntTaskWaiting)
{
    hycast::Task<int> task{[]{return 1;}};
    hycast::Future<int> future{task};
    hycast::Thread thread(future);
    EXPECT_FALSE(future.wasCanceled());
    EXPECT_EQ(1, future.getResult());
}

// Tests
TEST_F(TaskFutureTest, IntFutureCancelOrdering)
{
    hycast::Task<int> task{[]{return 1;}};
    hycast::Future<int> future{task};
    hycast::Thread thread(future);
    EXPECT_FALSE(future.wasCanceled());
    future.cancel();
    EXPECT_FALSE(future.wasCanceled());
    EXPECT_NO_THROW(future.getResult());
}

// Tests
TEST_F(TaskFutureTest, IntFutureExceptionOrdering)
{
    hycast::Task<int> task{[]{throw hycast::RuntimeError(__FILE__, __LINE__,
            "Hi there!"); return 1;}};
    hycast::Future<int> future{task};
    hycast::Thread thread(future);
    EXPECT_FALSE(future.wasCanceled());
    future.cancel();
    EXPECT_FALSE(future.wasCanceled());
    EXPECT_THROW(future.getResult(), hycast::RuntimeError);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
