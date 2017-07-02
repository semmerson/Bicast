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
    typedef hycast::Thread::ThreadId ThreadId;
    typedef std::mutex               Mutex;
    typedef std::lock_guard<Mutex>   LockGuard;
    typedef std::unique_lock<Mutex>  UniqueLock;

    std::mutex                mutex;
    std::condition_variable   cond;
    hycast::Thread::ThreadId  threadId;
    std::atomic_bool          cleanupCalled;
    bool                      callableCalled;

    ThreadTest()
        : mutex{}
        , cond{}
        , threadId{}
        , cleanupCalled{false}
        , callableCalled{false}
    {}

    void markAndNotify()
    {
        LockGuard lock{mutex};
        threadId = hycast::Thread::getId();
        callableCalled = true;
        cond.notify_one();
    }

    void printArgs(int one, int two, int three)
    {
        std::cout << "one=" << one << '\n';
        std::cout << "two=" << two << '\n';
        std::cout << "three=" << three << '\n';
        markAndNotify();
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

    void testCancelState()
    {
        bool enabled = hycast::Thread::enableCancel(false);
        EXPECT_TRUE(enabled);
        enabled = hycast::Thread::enableCancel(true);
        EXPECT_FALSE(enabled);
        markAndNotify();
        ::pause();
    }

    static void cleanup(void* arg)
    {
        ThreadTest* instance  = static_cast<ThreadTest*>(arg);
        EXPECT_NE(ThreadId{}, instance->threadId);
        EXPECT_FALSE(instance->cleanupCalled);
        instance ->cleanupCalled = true;
    }
};

#if 1
// Tests default construction
TEST_F(ThreadTest, DefaultConstruction)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{};
    ASSERT_EQ(0, hycast::Thread::size());
    EXPECT_THROW(thread.id(), hycast::InvalidArgument);
}
#endif

// Tests construction
TEST_F(ThreadTest, Construction)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{[]{}};
    ASSERT_EQ(1, hycast::Thread::size());
}

// Tests move construction
TEST_F(ThreadTest, MoveConstruction)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{hycast::Thread{[]{}}};
    ASSERT_EQ(1, hycast::Thread::size());
}

#if 1
// Tests move assignment
TEST_F(ThreadTest, MoveAssignment)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{[]{}};
    ASSERT_EQ(1, hycast::Thread::size());
    ThreadId threadId = thread.id();
    thread = hycast::Thread{[]{}};
    EXPECT_NE(threadId, thread.id());
    ASSERT_EQ(1, hycast::Thread::size());
}

// Tests id() of joined thread
TEST_F(ThreadTest, IdOfJoinedThread)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{[]{}};
    thread.join();
    EXPECT_THROW(thread.id(), hycast::InvalidArgument);
}

// Tests usage
TEST_F(ThreadTest, Usage)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{[this]{printArgs(1, 2, 3);}};
    ASSERT_EQ(1, hycast::Thread::size());
    waitOnCallable();
}

// Tests member cancellation
TEST_F(ThreadTest, MemberCancellation)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{[this]{markNotifyAndPause();}};
    auto id = thread.id();
    waitOnCallable();
    thread.cancel();
    thread.join();
    EXPECT_EQ(id, threadId);
    ASSERT_EQ(0, hycast::Thread::size());
}

// Tests non-member cancellation
TEST_F(ThreadTest, NonMemberCancellation)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{[this]{markNotifyAndPause();}};
    auto id = thread.id();
    waitOnCallable();
    hycast::Thread::cancel(thread.id());
    thread.join();
    EXPECT_EQ(id, threadId);
    ASSERT_EQ(0, hycast::Thread::size());
}

// Tests destructor cancellation
TEST_F(ThreadTest, DestructorCancellation)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{[]{::pause();}};
    ASSERT_EQ(1, hycast::Thread::size());
}

// Tests canceling completed thread
TEST_F(ThreadTest, CancelDoneThread)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{[]{}};
    ::usleep(250000); // Allows thread to complete
    thread.cancel();
    thread.join();
    ASSERT_EQ(0, hycast::Thread::size());
}

// Tests move assignment and non-member cancellation
TEST_F(ThreadTest, MoveAssignmentAndNonMemberCancellation)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{};
    thread = hycast::Thread{[this]{markNotifyAndPause();}};
    auto id = thread.id();
    waitOnCallable();
    EXPECT_NO_THROW(hycast::Thread::cancel(thread.id()));
    thread.join();
    ASSERT_EQ(0, hycast::Thread::size());
    EXPECT_TRUE(callableCalled);
    EXPECT_EQ(id, threadId);
}

// Tests setting cancelability
TEST_F(ThreadTest, SettingCancelability)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{[this]{testCancelState();}};
    waitOnCallable();
    thread.cancel();
    thread.join();
    ASSERT_EQ(0, hycast::Thread::size());
}

// Tests thread cleanup routine
TEST_F(ThreadTest, ThreadCleanupRoutine)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{};
    thread = hycast::Thread{[this] {
            THREAD_CLEANUP_PUSH(cleanup, this);
            markAndNotify();
            ::pause();
            THREAD_CLEANUP_POP(false);
    }};
    auto id = thread.id();
    waitOnCallable();
    EXPECT_NO_THROW(hycast::Thread::cancel(thread.id()));
    thread.join();
    EXPECT_NE(ThreadId{}, threadId);
    EXPECT_TRUE(cleanupCalled);
    ASSERT_EQ(0, hycast::Thread::size());
    EXPECT_EQ(id, threadId);
}
#endif
#if 1
// Tests cancellation loop
TEST_F(ThreadTest, CancellationLoop)
{
    //std::set_terminate([]{::pause();});
    int i;
    try {
        for (i = 0; i < 500; ++i) {
            ASSERT_EQ(0, hycast::Thread::size());
            hycast::Thread thread{[]{}};
            thread.cancel();
        }
        for (; i < 1000; ++i) {
            ASSERT_EQ(0, hycast::Thread::size());
            hycast::Thread thread{[this]{markNotifyAndPause();}};
            thread.cancel();
        }
        for (; i < 1500; ++i) {
            ASSERT_EQ(0, hycast::Thread::size());
            hycast::Thread thread{[this]{markNotifyAndPause();}};
            waitOnCallable();
            thread.cancel();
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Loop failed on iteration " << i << '\n';
    }
}
#endif
}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
