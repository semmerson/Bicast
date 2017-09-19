/**
 * This file tests class `std::thread`.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: thread_test.cpp
 * Created On: Jul 7, 2017
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include <atomic>
#include <condition_variable>
#include "gtest/gtest.h"
#include <mutex>
#include <pthread.h>
#include <thread>
#include <unistd.h>

namespace {

/// The fixture for testing class `std::thread`
class ThreadTest : public ::testing::Test
{
protected:
    typedef std::mutex              Mutex;
    typedef std::lock_guard<Mutex>  LockGuard;
    typedef std::unique_lock<Mutex> UniqueLock;
    typedef std::condition_variable Cond;

    Mutex mutex;
    Cond  cond;
    bool  callableCalled;

    ThreadTest()
        : callableCalled{false}
    {}

    void waitOnCallable()
    {
        UniqueLock lock{mutex};
        while (!callableCalled)
            cond.wait(lock);
    }

public:
    void run()
    {
        callableCalled = true;
    }

    void runArg(int arg)
    {
        callableCalled = true;
        EXPECT_EQ(1, arg);
    }

    void runRefArg(int& arg)
    {
        callableCalled = true;
        EXPECT_EQ(1, arg);
    }
};

// Tests using a member function
TEST_F(ThreadTest, MemberFunction)
{
    std::thread thread{&ThreadTest::run, this};
    thread.join();
    EXPECT_FALSE(thread.joinable());
    EXPECT_TRUE(callableCalled);
}

// Tests passing argument to a member function
TEST_F(ThreadTest, MemberFunctionWithArg)
{
    std::thread thread{&ThreadTest::runArg, this, 1};
    thread.join();
    EXPECT_FALSE(thread.joinable());
    EXPECT_TRUE(callableCalled);
}

// Tests passing argument reference to a member function
TEST_F(ThreadTest, MemberFunctionWithArgRef)
{
    const int arg = 1;
    std::thread thread{&ThreadTest::runArg, this, std::ref(arg)};
    thread.join();
    EXPECT_FALSE(thread.joinable());
    EXPECT_TRUE(callableCalled);
}

// Tests using a non-member function
TEST_F(ThreadTest, NonMemberFunction)
{
    std::thread thread{&::usleep, 250000};
    EXPECT_NO_THROW(thread.join());
    EXPECT_FALSE(thread.joinable());
}

// Tests canceling a thread
TEST_F(ThreadTest, CancelThread)
{
    std::thread thread{&::pause};
    EXPECT_TRUE(thread.joinable());
    EXPECT_EQ(0, ::pthread_cancel(thread.native_handle()));
    EXPECT_TRUE(thread.joinable());
    EXPECT_NO_THROW(thread.join());
    EXPECT_FALSE(thread.joinable());
}

// Tests canceling a bunch of threads
TEST_F(ThreadTest, CancelBunchOfThread)
{
    for (int i = 0; i < 1000; ++i) {
        std::thread thread{&::pause};
        EXPECT_TRUE(thread.joinable());
        EXPECT_EQ(0, ::pthread_cancel(thread.native_handle()));
        EXPECT_TRUE(thread.joinable());
        EXPECT_NO_THROW(thread.join());
        EXPECT_FALSE(thread.joinable());
    }
}

#if 0
// THIS DOESN'T WORK
// Tests passing argument reference to a reference member function
TEST_F(ThreadTest, RefMemberFunctionWithArgRef)
{
    const int arg = 1;
    std::thread thread{&ThreadTest::runRefArg, this, std::ref(arg)};
    thread.join();
    EXPECT_TRUE(callableCalled);
}
#endif

#if 0
/*
 * The following verifies that `terminate()` is called when a thread that's
 * handling an exception responds to being canceled. Unfortunately, running this
 * test necessarily terminates the program.
 */
static void myTerminate()
{
    std::cerr << "myTerminate() called\n"; // Must be printed for verification
    ::abort();
}

static void throwAndPause()
{
    try {
        throw std::runtime_error("Faux runtime error");
    }
    catch (const std::exception& ex) {
        ::pause();
    }
}

// Verifies that `terminate()` is called when a thread that's handling an
// exception responds to being canceled
TEST_F(ThreadTest, CancelExceptionHandling)
{
    std::set_terminate(&myTerminate);
    std::thread thread{throwAndPause};
    ::pthread_cancel(thread.native_handle());
    GTEST_FAIL(); // Never reached
}
#endif

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
