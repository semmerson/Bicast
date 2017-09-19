/**
 * This file tests class `Task`.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: Task_test.cpp
 * Created On: Jun 29, 2017
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "Task.h"
#include "Thread.h"

#include <condition_variable>
#include "gtest/gtest.h"
#include <iostream>
#include <mutex>
#include <unistd.h>

namespace {

/// The fixture for testing class `Task`
class TaskTest : public ::testing::Test
{
protected:
    typedef std::mutex               Mutex;
    typedef std::lock_guard<Mutex>   LockGuard;
    typedef std::unique_lock<Mutex>  UniqueLock;
    typedef std::condition_variable  Cond;

    Mutex                    mutex;
    Cond                     cond;
    bool                     taskStarted;
    bool                     callableCalled;
    hycast::Thread::Id       threadId;
    bool                     stopCalled;

    TaskTest()
        : mutex{}
        , cond{}
        , taskStarted{false}
        , callableCalled{false}
        , threadId{}
        , stopCalled{false}
    {}

    void stop(const bool mayInterrupt)
    {
        stopCalled = true;
    }

    void stop(
            const bool      mayInterrupt,
            hycast::Thread& thread)
    {
        stopCalled = true;
        thread.cancel();
    }

    void markAndNotify()
    {
        LockGuard lock{mutex};
        callableCalled = true;
        threadId = hycast::Thread::getId();
        cond.notify_one();
    }

    void markNotifyAndPause()
    {
        markAndNotify();
        hycast::Canceler canceler{};
        ::pause();
    }

    void waitOnCallable()
    {
        UniqueLock lock(mutex);
        while (!callableCalled) {
            hycast::Canceler canceler{};
            cond.wait(lock);
        }
    }
};

// Tests default construction of void task
TEST_F(TaskTest, VoidTaskDefaultConstruction)
{
    hycast::Task<void> task{};
    EXPECT_FALSE(task);
    EXPECT_THROW(task(), hycast::LogicError);
}

// Tests construction of trivial void task
TEST_F(TaskTest, VoidTaskConstruction)
{
    hycast::Task<void> task{[]{}};
    EXPECT_TRUE(task);
    auto future = task.getFuture();
    EXPECT_TRUE(future);
    auto thread = hycast::Thread(task);
    EXPECT_NO_THROW(future.getResult());
    EXPECT_NO_THROW(future.cancel());
    EXPECT_FALSE(future.wasCanceled());
    thread.join();
}

// Tests exception in void task
TEST_F(TaskTest, VoidTaskException)
{
    hycast::Thread thread;
    hycast::Task<void> task{[]{throw std::runtime_error("Dummy");}};
    auto future = task.getFuture();
    thread = hycast::Thread{task};
    try {
        future.getResult();
    }
    catch (const std::exception& e) {
        EXPECT_STREQ("Dummy", e.what());
    }
    EXPECT_TRUE(future.hasCompleted());
    EXPECT_FALSE(future.wasCanceled());
    thread.join();
}

// Tests cancellation of void task
TEST_F(TaskTest, VoidTaskCancellation)
{
    hycast::Thread thread;
    hycast::Task<void> task{[]{hycast::Canceler canceler{}; ::pause();}};
    auto future = task.getFuture();
    thread = hycast::Thread{task};
    EXPECT_FALSE(future.hasCompleted());
    EXPECT_NO_THROW(task.cancel());
    EXPECT_THROW(future.getResult(), hycast::LogicError);
    EXPECT_TRUE(future.hasCompleted());
    EXPECT_TRUE(future.wasCanceled());
    thread.join();
}

// Tests cancellation of void task via future
TEST_F(TaskTest, VoidFutureCancellation)
{
    hycast::Thread thread;
    hycast::Task<void> task{[]{hycast::Canceler canceler{}; ::pause();}};
    auto future = task.getFuture();
    thread = hycast::Thread{task};
    EXPECT_FALSE(future.hasCompleted());
    EXPECT_NO_THROW(future.cancel());
    EXPECT_THROW(future.getResult(), hycast::LogicError);
    EXPECT_TRUE(future.hasCompleted());
    EXPECT_TRUE(future.wasCanceled());
    thread.join();
}

// Tests looping over cancellation via future of void task
TEST_F(TaskTest, VoidTaskCancellationLoop)
{
    hycast::Thread thread;
    for (int i = 0; i < 1000; ++i) {
        hycast::Task<void> task{[]{hycast::Canceler canceler{}; ::pause();}};
        auto future = task.getFuture();
        thread = hycast::Thread{task};
        future.cancel();
        EXPECT_TRUE(future.wasCanceled());
        thread.join();
    }
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
