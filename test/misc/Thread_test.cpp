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
#include <random>

namespace {

/// The fixture for testing class `Thread`
class ThreadTest : public ::testing::Test
{
protected:
    typedef hycast::Thread::Id ThreadId;
    typedef std::mutex               Mutex;
    typedef std::lock_guard<Mutex>   LockGuard;
    typedef std::unique_lock<Mutex>  UniqueLock;

    std::mutex                mutex;
    std::condition_variable   cond;
    hycast::Thread::Id  threadId;
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
    std::cout << "Default thread-ID: " << hycast::Thread::Id{} << '\n';
    EXPECT_EQ(0, hycast::Thread::size());
    EXPECT_NO_THROW(thread.id());
    EXPECT_EQ(ThreadId{}, thread.id());
    EXPECT_NO_THROW(thread.cancel());
}
#endif

// Tests construction
TEST_F(ThreadTest, Construction)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{[]{}};
    EXPECT_EQ(1, hycast::Thread::size());
}

#if 0
// Tests copy construction
TEST_F(ThreadTest, CopyConstruction)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread1{[]{}};
    EXPECT_EQ(1, hycast::Thread::size());
    hycast::Thread thread2{thread1};
    EXPECT_EQ(1, hycast::Thread::size());
    EXPECT_EQ(thread1.id(), thread2.id());
}
#endif

// Tests move construction
TEST_F(ThreadTest, MoveConstruction)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{hycast::Thread{[]{}}};
    EXPECT_EQ(1, hycast::Thread::size());
}

// Tests copy assignment
TEST_F(ThreadTest, CopyAssignment)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread1{};
    hycast::Thread thread2{[]{}};
    EXPECT_NE(thread1.id(), thread2.id());
    EXPECT_EQ(1, hycast::Thread::size());
    thread1 = thread2; // Copy assignment
    EXPECT_EQ(thread1.id(), thread2.id());
    EXPECT_EQ(1, hycast::Thread::size());
}

#if 1
// Tests move assignment
TEST_F(ThreadTest, MoveAssignment)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{};
    ThreadId threadId = thread.id();
    thread = hycast::Thread{[]{}}; // Move assignment
    EXPECT_NE(threadId, thread.id());
    EXPECT_EQ(1, hycast::Thread::size());
}

// Tests id() of joined thread
TEST_F(ThreadTest, IdOfJoinedThread)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{[]{}};
    auto ident = thread.id();
    thread.join();
    EXPECT_NE(ident, thread.id());
    EXPECT_EQ(ThreadId{}, thread.id());
}

// Tests usage
TEST_F(ThreadTest, Usage)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{[this]{printArgs(1, 2, 3);}};
    EXPECT_EQ(1, hycast::Thread::size());
    thread.join();
    EXPECT_EQ(0, hycast::Thread::size());
}

// Tests instance cancellation
TEST_F(ThreadTest, InstanceCancellation)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{[this]{markNotifyAndPause();}};
    auto id = thread.id();
    waitOnCallable();
    thread.cancel();
    thread.join();
    EXPECT_EQ(id, threadId);
    EXPECT_EQ(0, hycast::Thread::size());
}

// Tests static cancellation
TEST_F(ThreadTest, StaticCancellation)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{[this]{markNotifyAndPause();}};
    auto id = thread.id();
    waitOnCallable();
    hycast::Thread::cancel(id);
    thread.join();
    EXPECT_EQ(id, threadId);
    EXPECT_EQ(0, hycast::Thread::size());
}

// Tests destructor cancellation
TEST_F(ThreadTest, DestructorCancellation)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{[]{::pause();}};
    EXPECT_EQ(1, hycast::Thread::size());
}

// Tests canceling completed thread
TEST_F(ThreadTest, CancelDoneThread)
{
    ASSERT_EQ(0, hycast::Thread::size());
    hycast::Thread thread{[this]{markAndNotify();}};
    waitOnCallable();
    thread.cancel();
    thread.join();
    EXPECT_EQ(0, hycast::Thread::size());
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
    EXPECT_EQ(0, hycast::Thread::size());
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
    EXPECT_EQ(0, hycast::Thread::size());
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
    EXPECT_EQ(0, hycast::Thread::size());
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
            thread.join();
        }
        for (; i < 1000; ++i) {
            ASSERT_EQ(0, hycast::Thread::size());
            hycast::Thread thread{[this]{markNotifyAndPause();}};
            thread.cancel();
            thread.join();
        }
        for (; i < 1500; ++i) {
            ASSERT_EQ(0, hycast::Thread::size());
            hycast::Thread thread{[this]{markNotifyAndPause();}};
            waitOnCallable();
            thread.cancel();
            thread.join();
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Loop failed on iteration " << i << '\n';
    }
}

// Tests execution of a bunch of threads
TEST_F(ThreadTest, BunchOfThreads) {
    //std::set_terminate([]{std::cout << "terminate() called\n"; ::pause();});
    std::default_random_engine generator{};
    std::uniform_int_distribution<useconds_t> distribution{0, 100000};
    hycast::Thread threads[200];
    for (int i = 0; i < 200; ++i)
        threads[i] = hycast::Thread([&generator,&distribution]() mutable
                {::usleep(distribution(generator));});
    usleep(50000);
}
#endif
}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
