/**
 * This file tests the class `Future`.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Future_test.cpp
 * @author: Steven R. Emmerson
 */


#include "Future.h"

#include <exception>
#include <gtest/gtest.h>
#include <iostream>
#include <pthread.h>
#include <stdexcept>
#include <stdio.h>
#include <thread>
#include <unistd.h>

namespace {

// The fixture for testing class Future.
class FutureTest : public ::testing::Test {
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    FutureTest() {
        // You can do set-up work for each test here.
    }

    virtual ~FutureTest() {
        // You can do clean-up work that doesn't throw exceptions here.
    }

    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:

    virtual void SetUp() {
        // Code here will be called immediately after the constructor (right
        // before each test).
    }

    virtual void TearDown() {
        // Code here will be called immediately after each test (right
        // before the destructor).
    }

    // Objects declared here can be used by all tests in the test case for Future.
};

// Tests default construction of void future
TEST_F(FutureTest, VoidFutureDefaultConstruction) {
    hycast::Future<void> future{};
}
// Tests default construction of int future
TEST_F(FutureTest, IntFutureDefaultConstruction) {
    hycast::Future<int> future{};
}

void* returnNullPtr(void* arg)
{
    return nullptr;
}

// Tests execution of void future
TEST_F(FutureTest, VoidFuture) {
    pthread_t thread;
    ASSERT_EQ(0, ::pthread_create(&thread, nullptr, returnNullPtr, nullptr));
    hycast::Future<void> future{thread};
    future.setResult();
    future.wait();
    EXPECT_FALSE(future.wasCancelled());
}

// Tests execution of int future
TEST_F(FutureTest, IntFuture) {
    pthread_t thread;
    ASSERT_EQ(0, ::pthread_create(&thread, nullptr, returnNullPtr, nullptr));
    hycast::Future<int> future{thread};
    future.setResult(1);
    future.wait();
    EXPECT_FALSE(future.wasCancelled());
    EXPECT_EQ(1, future.getResult());
}

// Tests move-construction of void future
TEST_F(FutureTest, VoidMoveConstruction) {
    pthread_t thread;
    ASSERT_EQ(0, ::pthread_create(&thread, nullptr, returnNullPtr, nullptr));
    hycast::Future<void> future1{thread};
    hycast::Future<void> future2{std::move(future1)};
    future2.setResult();
    future2.wait();
    EXPECT_FALSE(future2.wasCancelled());
}

// Tests move-construction of int future
TEST_F(FutureTest, IntMoveConstruction) {
    pthread_t thread;
    ASSERT_EQ(0, ::pthread_create(&thread, nullptr, returnNullPtr, nullptr));
    hycast::Future<int> future1{thread};
    hycast::Future<int> future2{std::move(future1)};
    future2.setResult(1);
    future2.wait();
    EXPECT_FALSE(future2.wasCancelled());
    EXPECT_EQ(1, future2.getResult());
}

// Tests equality of void futures
TEST_F(FutureTest, VoidFutureEquality) {
    pthread_t thread1;
    pthread_t thread2;
    ASSERT_EQ(0, ::pthread_create(&thread1, nullptr, returnNullPtr, nullptr));
    ASSERT_EQ(0, ::pthread_create(&thread2, nullptr, returnNullPtr, nullptr));
    hycast::Future<void> future1{thread1};
    hycast::Future<void> future2{thread2};
    EXPECT_TRUE(future1 == future1);
    EXPECT_TRUE(future2 == future2);
    EXPECT_FALSE(future1 == future2);
    EXPECT_FALSE(future2 == future1);
    future1.setResult();
    future2.setResult();
    future1.wait();
    future2.wait();
}

// Tests equality of int futures
TEST_F(FutureTest, IntFutureEquality) {
    pthread_t thread1;
    pthread_t thread2;
    ASSERT_EQ(0, ::pthread_create(&thread1, nullptr, returnNullPtr, nullptr));
    ASSERT_EQ(0, ::pthread_create(&thread2, nullptr, returnNullPtr, nullptr));
    hycast::Future<int> future1{thread1};
    hycast::Future<int> future2{thread2};
    EXPECT_TRUE(future1 == future1);
    EXPECT_TRUE(future2 == future2);
    EXPECT_FALSE(future1 == future2);
    EXPECT_FALSE(future2 == future1);
    future1.setResult(1);
    future2.setResult(2);
    future1.wait();
    future2.wait();
}

void* Pause(void* arg)
{
    try {
        ::pause();
    }
    catch (const std::exception& e) {
        ::fprintf(stderr, "Pause(): std::exception thrown: %s\n", e.what());
        throw;
    }
    catch (abi::__forced_unwind& e) {
        std::cerr << "Pause(): abi::__forced_unwind thrown\n";
        throw;
    }
    catch (...) {
        ::fprintf(stderr, "Pause(): ... exception thrown\n");
        throw;
    }
    return arg;
}

// Tests exception thrown by void future
TEST_F(FutureTest, VoidFutureException) {
    pthread_t thread;
    ASSERT_EQ(0, ::pthread_create(&thread, nullptr, Pause, nullptr));
    hycast::Future<void> future{thread};
    future.setException(std::make_exception_ptr(std::runtime_error("Dummy")));
    future.wait();
    EXPECT_FALSE(future.wasCancelled());
    EXPECT_THROW(future.getResult(), std::runtime_error);
}

// Tests exception thrown by int future
TEST_F(FutureTest, IntFutureException) {
    pthread_t thread;
    ASSERT_EQ(0, ::pthread_create(&thread, nullptr, Pause, nullptr));
    hycast::Future<int> future{thread};
    future.setException(std::make_exception_ptr(std::runtime_error("Dummy")));
    future.wait();
    EXPECT_FALSE(future.wasCancelled());
    EXPECT_THROW(future.getResult(), std::runtime_error);
}

// Tests cancellation of void task
TEST_F(FutureTest, CancelVoidFuture) {
    pthread_t thread;
    ASSERT_EQ(0, ::pthread_create(&thread, nullptr, Pause, nullptr));
    hycast::Future<void> future{thread};
    future.cancel();
    future.wait();
    EXPECT_TRUE(future.wasCancelled());
    EXPECT_THROW(future.getResult(), std::logic_error);
}

// Tests cancellation of int task
TEST_F(FutureTest, CancelLongRunningIntFuture) {
    pthread_t thread;
    ASSERT_EQ(0, ::pthread_create(&thread, nullptr, Pause, nullptr));
    hycast::Future<int> future{thread};
    future.cancel();
    future.wait();
    EXPECT_TRUE(future.wasCancelled());
    EXPECT_THROW(future.getResult(), std::logic_error);
}

#if 0
#endif

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
