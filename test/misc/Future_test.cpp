/**
 * This file tests class `Future`.
 *
 *   @file: Future_test.cpp
 * @author: Steven R. Emmerson
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

#include "error.h"
#include "Future.h"
#include "Thread.h"

#include <atomic>
#include <condition_variable>
#include <exception>
#include <gtest/gtest.h>
#include <iostream>
#include <mutex>
#include <pthread.h>
#include <random>
#include <stdexcept>
#include <stdio.h>
#include <thread>
#include <unistd.h>

namespace {

// The fixture for testing class Future.
class FutureTest : public ::testing::Test
{
protected:
    bool stopCalled;

    FutureTest()
        : stopCalled{false}
    {}

    void stop(const bool mayInterrupt = true)
    {
        stopCalled = true;
    }
};

// Tests default construction
TEST_F(FutureTest, DefaultConstruction)
{
    hycast::Future<void> future{};
    EXPECT_FALSE(future);
    EXPECT_FALSE(future.wasCanceled());
    EXPECT_THROW(future.cancel(), hycast::LogicError);
    EXPECT_THROW(future.cancel(false), hycast::LogicError);
    EXPECT_THROW(future.setResult(), hycast::LogicError);
    EXPECT_THROW(future.setException(
            std::make_exception_ptr(std::out_of_range("test"))),
            hycast::LogicError);
}

/******************************************************************************/

// Tests relational operators between void futures
TEST_F(FutureTest, VoidFutureRelationalOperations)
{
    hycast::Future<void> future1{[this](bool mayInterrupt){stop(mayInterrupt);}};
    EXPECT_TRUE(future1);
    EXPECT_TRUE(future1 == future1);

    hycast::Future<void> future2{[this](bool mayInterrupt){stop(mayInterrupt);}};
    EXPECT_TRUE(future2);
    EXPECT_TRUE(future1 != future2);
    EXPECT_TRUE(future1 < future2);
}

// Tests setting canceled flag of void future
TEST_F(FutureTest, VoidFutureSetCancelFlag)
{
    hycast::Future<void> future{[this](bool mayInterrupt){stop(mayInterrupt);}};
    future.setCanceled();
    EXPECT_TRUE(future.wasCanceled());
}

// Tests waiting on void future
TEST_F(FutureTest, WaitingOnVoidFuture)
{
    hycast::Future<void> future{[this](bool mayInterrupt){stop(mayInterrupt);}};
    auto thread = std::thread([&future]{usleep(100000); future.setResult();});
    future.wait();
    EXPECT_FALSE(future.wasCanceled());
    EXPECT_NO_THROW(future.getResult());
    future.cancel();
    EXPECT_FALSE(future.wasCanceled());
    thread.join();
}

// Tests hard cancellation of void future
TEST_F(FutureTest, VoidFutureHardCancellation)
{
    bool stopCalled{false};
    bool interrupted{false};
    hycast::Future<void> future{[&stopCalled,&interrupted,&future]
            (bool mayInterrupt) {
        stopCalled = true;
        interrupted = mayInterrupt;
        if (mayInterrupt)
            future.setCanceled();
    }};
    future.cancel();
    EXPECT_TRUE(stopCalled);
    EXPECT_TRUE(interrupted);
    EXPECT_TRUE(future.hasCompleted());
    EXPECT_THROW(future.getResult(), hycast::LogicError);
    EXPECT_TRUE(future.wasCanceled());
}

// Tests soft cancellation of void future
TEST_F(FutureTest, VoidFutureSoftCancellation)
{
    bool stopCalled{false};
    bool interrupted{false};
    hycast::Future<void> future{[&stopCalled,&interrupted,&future]
            (bool mayInterrupt) {
        stopCalled = true;
        interrupted = mayInterrupt;
        if (mayInterrupt)
            future.setCanceled();
    }};
    future.cancel(false);
    EXPECT_TRUE(stopCalled);
    EXPECT_FALSE(interrupted);
    EXPECT_FALSE(future.hasCompleted());
}

// Tests setting exception for void future
TEST_F(FutureTest, VoidFutureException)
{
    hycast::Future<void> future{[this](bool mayInterrupt){stop(mayInterrupt);}};
    auto thread = std::thread([&future]{usleep(100000);
            future.setException(
                    std::make_exception_ptr(std::out_of_range("test")));});
    EXPECT_THROW(future.getResult(), std::out_of_range);
    EXPECT_FALSE(stopCalled);
    EXPECT_FALSE(future.wasCanceled());
    thread.join();
}

/******************************************************************************/

// Tests relational operators between int futures
TEST_F(FutureTest, IntFutureRelationalOperations)
{
    hycast::Future<int> future1{[this](bool mayInterrupt){stop(mayInterrupt);}};
    EXPECT_TRUE(future1);
    EXPECT_TRUE(future1 == future1);

    hycast::Future<int> future2{[this](bool mayInterrupt){stop(mayInterrupt);}};
    EXPECT_TRUE(future2);
    EXPECT_TRUE(future1 != future2);
    EXPECT_TRUE(future1 < future2);
}

// Tests setting canceled flag of int future
TEST_F(FutureTest, IntFutureSetCancelFlag)
{
    hycast::Future<int> future{[this](bool mayInterrupt){stop(mayInterrupt);}};
    future.setCanceled();
    EXPECT_TRUE(future.wasCanceled());
}

// Tests waiting on int future
TEST_F(FutureTest, WaitingOnIntFuture)
{
    hycast::Future<int> future{[this](bool mayInterrupt){stop(mayInterrupt);}};
    auto thread = std::thread([&future]{usleep(100000); future.setResult(1);});
    future.wait();
    EXPECT_FALSE(future.wasCanceled());
    EXPECT_EQ(1, future.getResult());
    future.cancel();
    EXPECT_FALSE(future.wasCanceled());
    thread.join();
}

// Tests hard cancellation of int future
TEST_F(FutureTest, IntFutureHardCancellation)
{
    bool stopCalled{false};
    bool interrupted{false};
    hycast::Future<int> future{[&stopCalled,&interrupted,&future]
            (bool mayInterrupt) {
        stopCalled = true;
        interrupted = mayInterrupt;
        if (mayInterrupt)
            future.setCanceled();
    }};
    future.cancel();
    EXPECT_TRUE(stopCalled);
    EXPECT_TRUE(interrupted);
    EXPECT_TRUE(future.hasCompleted());
    EXPECT_THROW(future.getResult(), hycast::LogicError);
    EXPECT_TRUE(future.wasCanceled());
}

// Tests soft cancellation of int future
TEST_F(FutureTest, IntFutureSoftCancellation)
{
    bool stopCalled{false};
    bool interrupted{false};
    hycast::Future<int> future{[&stopCalled,&interrupted,&future]
            (bool mayInterrupt) {
        stopCalled = true;
        interrupted = mayInterrupt;
        if (mayInterrupt)
            future.setCanceled();
    }};
    future.cancel(false);
    EXPECT_TRUE(stopCalled);
    EXPECT_FALSE(interrupted);
    EXPECT_FALSE(future.hasCompleted());
}

// Tests setting exception for int future
TEST_F(FutureTest, IntFutureException)
{
    hycast::Future<int> future{[this](bool mayInterrupt){stop(mayInterrupt);}};
    auto thread = std::thread([&future]{usleep(100000);
            future.setException(
                    std::make_exception_ptr(std::out_of_range("test")));});
    EXPECT_THROW(future.getResult(), std::out_of_range);
    EXPECT_FALSE(stopCalled);
    EXPECT_FALSE(future.wasCanceled());
    thread.join();
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
