/**
 * This file tests the interaction of "noexcept", GTest, and
 * thread-cancellation.
 *
 *       File: noexcept_test.cpp
 * Created On: Sep 25, 2019
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

#include <condition_variable>
#include "gtest/gtest.h"
#include <mutex>
#include <pthread.h>
#include <thread>

namespace {

typedef std::mutex              Mutex;
typedef std::lock_guard<Mutex>  Guard;
typedef std::unique_lock<Mutex> Lock;
typedef std::condition_variable Cond;

/// The fixture for testing class `Noexcept`
class NoexceptTest : public ::testing::Test
{
protected:

    Mutex mutex;
    Cond  cond;
    bool  ready;

    // You can remove any or all of the following functions if its body
    // is empty.

    NoexceptTest()
        : mutex()
        , cond()
        , ready{false}
    {
        // You can do set-up work for each test here.
    }

    virtual ~NoexceptTest()
    {
        // You can do clean-up work that doesn't throw exceptions here.
    }

    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:

    virtual void SetUp()
    {
        // Code here will be called immediately after the constructor (right
        // before each test).
    }

    virtual void TearDown()
    {
        // Code here will be called immediately after each test (right
        // before the destructor).
    }

    // Objects declared here can be used by all tests in the test case for Error.

    void exceptPause() {
        {
            Guard guard{mutex};
            ready = true;
            cond.notify_one();
        }
        ::pause();
    }

    void noexceptPause() noexcept {
        {
            Guard guard{mutex};
            ready = true;
            cond.notify_one();
        }
        ::pause();
    }
};

// Tests canceling a regular thread
TEST_F(NoexceptTest, CancelExceptThread)
{
    std::thread thread(&NoexceptTest_CancelExceptThread_Test::exceptPause,
            this);

    Lock lock{mutex};
    while (!ready)
        cond.wait(lock);

    ::pthread_cancel(thread.native_handle());
    thread.join();
}

// Tests canceling a "noexcept" thread. Calls `std::terminate()` with no
// exception.
#if 0
TEST_F(NoexceptTest, CancelNoexceptThread)
{
    std::thread thread(&NoexceptTest_CancelNoexceptThread_Test::noexceptPause,
            this);

    Lock lock{mutex};
    while (!ready)
        cond.wait(lock);

    ::pthread_cancel(thread.native_handle());
    thread.join();
}
#endif

}  // namespace

class ExceptJob
{
public:
    void operator()(Mutex& mutex, Cond& cond, bool& ready)
    {
        {
            Guard guard{mutex};
            ready = true;
            cond.notify_one();
        }
        ::pause();
    }

    ~ExceptJob()
    {}
};

// Tests canceling a thread that's executing a class object that might throw.
TEST_F(NoexceptTest, CancelClassObject)
{
    ExceptJob   job;
    std::thread thread(&ExceptJob::operator(), &job, std::ref(mutex),
            std::ref(cond), std::ref(ready));

    Lock lock{mutex};
    while (!ready)
        cond.wait(lock);

    ::pthread_cancel(thread.native_handle());
    thread.join();
}

class NoexceptJob
{
public:
    void operator()(Mutex& mutex, Cond& cond, bool& ready) noexcept
    {
        {
            Guard guard{mutex};
            ready = true;
            cond.notify_one();
        }
        ::pause();
    }
};

// Tests canceling a thread that's executing a noexcept class object. Calls
// `std::terminate()` without an active exception
#if 0
TEST_F(NoexceptTest, CancelNoexceptClassObject)
{
    NoexceptJob   job;
    std::thread thread(&NoexceptJob::operator(), &job, std::ref(mutex),
            std::ref(cond), std::ref(ready));

    Lock lock{mutex};
    while (!ready)
        cond.wait(lock);

    ::pthread_cancel(thread.native_handle());
    thread.join();
}
#endif

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
