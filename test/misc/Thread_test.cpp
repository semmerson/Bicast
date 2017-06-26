/**
 * This file tests class `Thread`.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: Thread_test.cpp
 * Created On: May 18, 2017
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "Thread.h"

#include <atomic>
#include <climits>
#include <condition_variable>
#include <gtest/gtest.h>
#include <iostream>
#include <mutex>

namespace {

/// The fixture for testing class `Thread`
class ThreadTest : public ::testing::Test
{
protected:
    typedef std::mutex              Mutex;
    typedef std::lock_guard<Mutex>  LockGuard;
    typedef std::unique_lock<Mutex> UniqueLock;

    std::mutex                mutex;
    std::condition_variable   cond;
    hycast::Thread::ThreadId  threadId;
    bool                      threadStarted;
    std::atomic_bool          cleanupCalled;

    ThreadTest()
        : mutex{}
        , cond{}
        , threadId{0}
        , threadStarted{false}
        , cleanupCalled{false}
    {}

    void printArgs(int one, int two, int three)
    {
        std::cout << "one=" << one << '\n';
        std::cout << "two=" << two << '\n';
        std::cout << "three=" << three << '\n';
    }

    void testCancelState()
    {
        bool enabled = hycast::Thread::enableCancel(false);
        EXPECT_TRUE(enabled);
        enabled = hycast::Thread::enableCancel(true);
        EXPECT_FALSE(enabled);
        ::pause();
    }

    static void cleanup(void* arg)
    {
        ThreadTest* instance  = static_cast<ThreadTest*>(arg);
        EXPECT_NE(0, instance->threadId);
        EXPECT_TRUE(instance->threadStarted);
        EXPECT_FALSE(instance->cleanupCalled);
        instance ->cleanupCalled = true;
    }
};

// Tests default construction
TEST_F(ThreadTest, DefaultConstruction)
{
    hycast::Thread thread{};
}

// Tests move construction
// WON'T COMPILE
TEST_F(ThreadTest, MoveConstruction)
{
    hycast::Thread thread{std::move(hycast::Thread{[]{}})};
}

// Tests move assignment
TEST_F(ThreadTest, MoveAssignment)
{
    hycast::Thread thread{};
    pthread_t threadId = thread.native_handle();
    thread = hycast::Thread{[]{}};
    EXPECT_NE(threadId, thread.native_handle());
}

// Tests usage
TEST_F(ThreadTest, Usage)
{
    hycast::Thread thread{[this]{printArgs(1, 2, 3);}};
}

// Tests default cancellation
TEST_F(ThreadTest, DefaultCancellation)
{
    hycast::Thread thread{[]{::pause();}};
    ::usleep(250000); // Allows thread to start
}

// Tests member cancellation
TEST_F(ThreadTest, MemberCancellation)
{
    hycast::Thread thread{::pause};
    ::usleep(250000); // Allows thread to start
    thread.cancel();
}

// Tests non-member cancellation
TEST_F(ThreadTest, NonMemberCancellation)
{
    hycast::Thread thread{::pause};
    ::usleep(250000); // Allows thread to start
    hycast::Thread::cancel(thread.native_handle());
}

// Tests move assignment and non-member cancellation
TEST_F(ThreadTest, MoveAssignmentAndNonMemberCancellation)
{
    hycast::Thread thread{};
    thread = hycast::Thread{[]{::pause();}};
    ::usleep(250000); // Allows thread to start
    EXPECT_NO_THROW(hycast::Thread::cancel(thread.native_handle()));
}

// Tests setting cancelability
TEST_F(ThreadTest, SettingCancelability)
{
    hycast::Thread thread{[this]{testCancelState();}};
    ::usleep(250000);
    thread.cancel();
}

// Tests thread cleanup routine
TEST_F(ThreadTest, ThreadCleanupRoutine)
{
    hycast::Thread thread{};
    thread = hycast::Thread{[this] {
            THREAD_CLEANUP_PUSH(cleanup, this);
            {
                LockGuard lock{this->mutex};
                this->threadId = hycast::Thread::getId();
                this->threadStarted = true;
                this->cond.notify_one();
            }
            ::pause();
            THREAD_CLEANUP_POP(false);
    }};
    {
        UniqueLock lock{mutex};
        while (!threadStarted)
            cond.wait(lock);
    }
    EXPECT_NO_THROW(hycast::Thread::cancel(thread.native_handle()));
    EXPECT_NO_THROW(thread.join());
    EXPECT_NE(0, threadId);
    EXPECT_TRUE(threadStarted);
    EXPECT_TRUE(cleanupCalled);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
