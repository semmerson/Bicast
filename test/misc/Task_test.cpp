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
    hycast::Thread::Id threadId;

    TaskTest()
        : mutex{}
        , cond{}
        , taskStarted{false}
        , callableCalled{false}
        , threadId{}
    {}

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
        ::pause();
    }

    void waitOnCallable()
    {
        UniqueLock lock(mutex);
        while (!callableCalled)
            cond.wait(lock);
    }
};

// Tests default construction of void task
TEST_F(TaskTest, VoidTaskDefaultConstruction)
{
    hycast::Task<void> task{};
    EXPECT_FALSE(task);
}

// Tests construction of trivial void task
TEST_F(TaskTest, VoidTaskConstruction)
{
    hycast::Task<void> task{[]{}};
    EXPECT_TRUE(task);
}

// Tests default cancellation of trivial void task
TEST_F(TaskTest, TrivialVoidTaskDefaultCancellation)
{
    EXPECT_EQ(0, hycast::Thread::size());
    hycast::Task<void> task{[]{}};
    hycast::Thread thread{task};
    EXPECT_EQ(1, hycast::Thread::size());
}

// Tests default cancellation of endless void task
TEST_F(TaskTest, EndlessVoidTaskDefaultCancellation)
{
    EXPECT_EQ(0, hycast::Thread::size());
    hycast::Task<void> task{[this]{markNotifyAndPause();}};
    hycast::Thread thread{task};
    waitOnCallable();
    EXPECT_TRUE(callableCalled);
    EXPECT_EQ(threadId, thread.id());
    EXPECT_EQ(1, hycast::Thread::size());
}

// Tests explicit cancellation of trivial void task
TEST_F(TaskTest, TrivialVoidTaskExplicitCancellation)
{
    EXPECT_EQ(0, hycast::Thread::size());
    hycast::Task<void> task{[]{}};
    hycast::Thread thread{task};
    EXPECT_EQ(1, hycast::Thread::size());
    thread.cancel();
    thread.join();
    EXPECT_EQ(0, hycast::Thread::size());
}

// Tests explicit cancellation of endless void task
TEST_F(TaskTest, EndlessVoidTaskExplicitCancellation)
{
    EXPECT_EQ(0, hycast::Thread::size());
    hycast::Task<void> task{[this]{markNotifyAndPause();}};
    hycast::Thread thread{task};
    waitOnCallable();
    EXPECT_TRUE(callableCalled);
    EXPECT_EQ(threadId, thread.id());
    EXPECT_EQ(1, hycast::Thread::size());
    thread.cancel();
    thread.join();
    EXPECT_EQ(0, hycast::Thread::size());
}

// Tests looping over explicit cancellation of endless void task
TEST_F(TaskTest, EndlessVoidTaskExplicitCancellationLoop)
{
    for (int i = 0; i < 1000; ++i) {
        EXPECT_EQ(0, hycast::Thread::size());
        std::cout << i << '\n';
        hycast::Task<void> task{[this]{markNotifyAndPause();}};
        hycast::Thread thread{task};
        waitOnCallable();
        EXPECT_TRUE(callableCalled);
        EXPECT_EQ(threadId, thread.id());
        EXPECT_EQ(1, hycast::Thread::size());
        thread.cancel();
        thread.join();
        EXPECT_EQ(0, hycast::Thread::size());
    }
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
