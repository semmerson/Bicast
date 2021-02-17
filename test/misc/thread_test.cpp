/**
 * This file tests class `std::thread`.
 *
 *       File: thread_test.cpp
 * Created On: Jul 7, 2017
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

#include "error.h"

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

// Tests canceling a terminated thread
TEST_F(ThreadTest, CancelTerminatedThread)
{
    std::thread thread{&::usleep, 1};
    ::usleep(100000);
    EXPECT_EQ(ESRCH, ::pthread_cancel(thread.native_handle())); // No such process
    EXPECT_TRUE(thread.joinable());
    EXPECT_NO_THROW(thread.join());
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

// Tests detaching a thread
// NB: A detached thread can't be canceled because it no longer has a valid ID.
TEST_F(ThreadTest, DetachThread)
{
    std::thread thread{&::pause};
    EXPECT_TRUE(thread.joinable());
    auto id = thread.get_id();
    auto nativeId = thread.native_handle();
    EXPECT_NE(0, nativeId);
    thread.detach();
    EXPECT_FALSE(thread.joinable());
    EXPECT_NE(id, thread.get_id()); // Oofta!
    EXPECT_NE(nativeId, thread.native_handle()); // Oofta!
    EXPECT_EQ(0, thread.native_handle());
    EXPECT_THROW(thread.join(), std::exception);
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
 * handling an exception is canceled. Unfortunately, running this test
 * necessarily terminates the program.
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
// exception is canceled
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
