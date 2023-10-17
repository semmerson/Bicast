/**
 * This file tests class `Task`.
 *
 *       File: Task_test.cpp
 * Created On: Jun 29, 2017
 *     Author: Steven R. Emmerson
 *
 *    Copyright 2021 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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

using namespace bicast;

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
    Thread::Tag               threadId;
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
            const bool mayInterrupt,
            Thread&    thread)
    {
        stopCalled = true;
        thread.cancel();
    }

    void markAndNotify()
    {
        LockGuard lock{mutex};
        callableCalled = true;
        threadId = Thread::getId();
        cond.notify_one();
    }

    void markNotifyAndPause()
    {
        markAndNotify();
        Canceler canceler{};
        ::pause();
    }

    void waitOnCallable()
    {
        UniqueLock lock(mutex);
        while (!callableCalled) {
            Canceler canceler{};
            cond.wait(lock);
        }
    }
};

// Tests default construction of void task
TEST_F(TaskTest, VoidTaskDefaultConstruction)
{
    Task<void> task{};
    EXPECT_FALSE(task);
    EXPECT_THROW(task(), LogicError);
}

// Tests construction of trivial void task
TEST_F(TaskTest, VoidTaskConstruction)
{
    Task<void> task{[]{}};
    EXPECT_TRUE(task);
    auto future = task.getFuture();
    EXPECT_TRUE(future);
    auto thread = Thread(task);
    EXPECT_NO_THROW(future.getResult());
    EXPECT_NO_THROW(future.cancel());
    EXPECT_FALSE(future.wasCanceled());
    thread.join();
}

// Tests exception in void task
TEST_F(TaskTest, VoidTaskException)
{
    Thread thread;
    Task<void> task{[]{throw std::runtime_error("Dummy");}};
    auto future = task.getFuture();
    thread = Thread{task};
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
    Thread thread;
    Task<void> task{[]{Canceler canceler{}; ::pause();}};
    auto future = task.getFuture();
    thread = Thread{task};
    EXPECT_FALSE(future.hasCompleted());
    EXPECT_NO_THROW(task.cancel());
    EXPECT_THROW(future.getResult(), LogicError);
    EXPECT_TRUE(future.hasCompleted());
    EXPECT_TRUE(future.wasCanceled());
    thread.join();
}

// Tests cancellation of void task via future
TEST_F(TaskTest, VoidFutureCancellation)
{
    Thread thread;
    Task<void> task{[]{Canceler canceler{}; ::pause();}};
    auto future = task.getFuture();
    thread = Thread{task};
    EXPECT_FALSE(future.hasCompleted());
    EXPECT_NO_THROW(future.cancel());
    EXPECT_THROW(future.getResult(), LogicError);
    EXPECT_TRUE(future.hasCompleted());
    EXPECT_TRUE(future.wasCanceled());
    thread.join();
}

// Tests looping over cancellation via future of void task
TEST_F(TaskTest, VoidTaskCancellationLoop)
{
    Thread thread;
    for (int i = 0; i < 1000; ++i) {
        Task<void> task{[]{Canceler canceler{}; ::pause();}};
        auto future = task.getFuture();
        thread = Thread{task};
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
