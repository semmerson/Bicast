/**
 * This file tests the start/stop/wait asynchronous task idiom
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: StartIdiom_test.cpp
 * Created On: Oct 2, 2020
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include <condition_variable>
#include <exception>
#include <gtest/gtest.h>
#include <pthread.h>
#include <thread>
#include <unistd.h>

namespace {

class Signal
{
    using Mutex = std::mutex;
    using Guard = std::lock_guard<Mutex>;
    using Lock  = std::unique_lock<Mutex>;
    using Cond  = std::condition_variable;
    using ExPtr = std::exception_ptr;

    Mutex mutex;
    Cond  cond;
    bool  done;
    ExPtr exPtr;

    inline void markDone() {
        done = true;
        cond.notify_all();
    }

public:
    Signal()
        : mutex{}
        , cond{}
        , done{false}
        , exPtr{}
    {}

    void setException(const std::exception& ex) {
        Guard guard{mutex};
        if (!exPtr) {
            exPtr = std::make_exception_ptr(ex);
            markDone();
        }
    }

    void setDone() {
        Guard guard{mutex};
        markDone();
    }

    void wait() {
        Lock lock{mutex};
        while (!done)
            cond.wait(lock);
        if (exPtr)
            std::rethrow_exception(exPtr);
    }
};

class Task
{
    std::thread thread;
    Signal&     signal;

public:
    Task(Signal& signal)
        : signal{signal}
    {}

    void start() {
        thread = std::thread(&::pause);
    }

    void stop() {
        EXPECT_EQ(0, ::pthread_cancel(thread.native_handle()));
    }

    void wait() {
        signal.wait();
        thread.join();
    }
};

class StartTest : public ::testing::Test
{
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    StartTest()
    {
        // You can do set-up work for each test here.
    }

    virtual ~StartTest()
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
};

// Tests idiom
TEST_F(StartTest, Idiom)
{
    Task task;
    task.start();
    task.stop();
    task.wait();
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
