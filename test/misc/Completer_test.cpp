/**
 * This file tests the class `Completer`.
 *
 *   @file: Completer_test.cpp
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

#include "Completer.h"
#include "error.h"
#include "Thread.h"

#include <array>
#include <gtest/gtest.h>
#include <random>
#include <unistd.h>

namespace {

// The fixture for testing class Completer.
class CompleterTest : public ::testing::Test {
protected:
    void cancelGet()
    {
        hycast::Completer<void> completer{};
        completer.submit([]{hycast::Canceler canceler{}; ::pause();});
        auto future = completer.take();
    }
};

// Tests construction of void completer
TEST_F(CompleterTest, VoidConstruction) {
    hycast::Completer<void> completer{};
}

// Tests construction of int completer
TEST_F(CompleterTest, IntConstruction) {
    hycast::Completer<int> completer{};
}

// Tests execution of void task
TEST_F(CompleterTest, VoidExecution) {
    hycast::Completer<void> completer{};
    auto future1 = completer.submit([]{});
    auto future2 = completer.take();
    EXPECT_TRUE(future1 == future2);
    EXPECT_FALSE(future2.wasCanceled());
    EXPECT_NO_THROW(future2.getResult());
}

// Tests execution of int task
TEST_F(CompleterTest, IntExecution) {
    hycast::Completer<int> completer{};
    auto future1 = completer.submit([]{return 1;});
    auto future2 = completer.take();
    EXPECT_TRUE(future1 == future2);
    EXPECT_FALSE(future2.wasCanceled());
    EXPECT_EQ(1, future2.getResult());
}

// Tests execution of multiple void tasks
TEST_F(CompleterTest, MultipleVoidExecution) {
    hycast::Completer<void> completer{};
    std::array<hycast::Future<void>, 8> futures;
    for (unsigned i = 0; i < futures.size(); ++i) {
        auto future = completer.submit([]{});
        futures[i] = future;
    }
    for (unsigned i = 0; i < futures.size(); ++i) {
        auto future = completer.take();
        EXPECT_FALSE(future.wasCanceled());
        EXPECT_NO_THROW(future.getResult());
    }
}

// Tests execution of multiple int tasks
TEST_F(CompleterTest, MultipleIntExecution) {
    hycast::Completer<int> completer{};
    std::array<hycast::Future<int>, 8> futures;
    for (unsigned i = 0; i < futures.size(); ++i)
        futures[i] = completer.submit([i]{return i;});
    for (unsigned i = 0; i < futures.size(); ++i) {
        auto future = completer.take();
        EXPECT_FALSE(future.wasCanceled());
        int j = future.getResult();
        EXPECT_EQ(futures[j], future);
    }
}

// Tests cancellation of void task
TEST_F(CompleterTest, VoidCancellation) {
    hycast::Completer<void> completer{};
    auto future = completer.submit([]{hycast::Canceler canceler{};
            ::pause();});
    future.cancel();
    EXPECT_TRUE(future.wasCanceled());
    EXPECT_THROW(future.getResult(), hycast::LogicError);
}

// Tests cancellation of int task
TEST_F(CompleterTest, IntCancellation) {
    hycast::Completer<int> completer{};
    auto future = completer.submit([]{hycast::Canceler canceler{}; ::pause();
            return 1;});
    future.cancel();
    EXPECT_TRUE(future.wasCanceled());
    EXPECT_THROW(future.getResult(), hycast::LogicError);
}

// Tests destruction of completer with active task
TEST_F(CompleterTest, DestructionWithTask) {
    hycast::Completer<void> completer{};
    completer.submit([]{hycast::Canceler canceler{}; ::pause();});
}

// Tests destruction of completer with active future
TEST_F(CompleterTest, DestructionWithFuture) {
    hycast::Completer<void> completer{};
    auto future = completer.submit([]{hycast::Canceler canceler{}; ::pause();});
}

// Tests cancellation of nested Completer::get()
TEST_F(CompleterTest, CancelNestedGet) {
    hycast::Completer<void> completer{};
    completer.submit([this]{cancelGet();});
    ::usleep(100000);
}

// Tests execution of a bunch of tasks
TEST_F(CompleterTest, BunchOfJobs) {
    //std::set_terminate([]{std::cout << "terminate() called\n";
    //          hycast::Canceler canceler{}; ::pause();});
    hycast::Completer<void> completer{};
    std::default_random_engine generator{};
    std::uniform_int_distribution<useconds_t> distribution{0, 100000};
    for (int i = 0; i < 200; ++i)
        completer.submit([&generator,&distribution]() mutable
                {::usleep(distribution(generator));});
    for (int i = 0; i < 100; ++i)
        completer.take();
}

// Tests completer destruction with a bunch of outstanding jobs
TEST_F(CompleterTest, DestructionWithOutstandingJobs) {
    hycast::Completer<void> completer{};
    for (int i = 0; i < 200; ++i)
        completer.submit([]{hycast::Canceler canceler{}; ::pause();});
}

static void subCompleter(hycast::Barrier& barrier) {
    hycast::Completer<void> completer{};
    auto future1 = completer.submit([]{hycast::Canceler canceler{}; ::pause();});
    auto future2 = completer.submit([]{hycast::Canceler canceler{}; ::pause();});
    barrier.wait();
    completer.take();
}

// Tests PeerSet completer usage
TEST_F(CompleterTest, PeerSetUsage) {
    hycast::Barrier barrier{2};
    hycast::Completer<void> completer{};
    completer.submit([&barrier]{subCompleter(barrier);});
    barrier.wait();
}

static void testDestructionTermination(useconds_t sleep) {
    {
        hycast::Completer<void> completer{};
        int n;
        for (n = 0; n < 10; ++n)
            completer.submit([]{hycast::Canceler canceler{}; ::pause();});
        if (sleep)
            ::usleep(sleep);
        EXPECT_EQ(n, hycast::Thread::size());
    }
    EXPECT_EQ(0, hycast::Thread::size());
}

// Tests guarantee that destruction terminates all threads
TEST_F(CompleterTest, DestructionTerminatesThreads) {
    testDestructionTermination(0);
    testDestructionTermination(200000);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
