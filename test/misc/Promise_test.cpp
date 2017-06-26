/**
 * This file tests class `Promise`.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: Promise_test.cpp
 * Created On: Jun 5, 2017
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "error.h"
#include "Promise.h"

#include "gtest/gtest.h"
#include <thread>

namespace {

/// The fixture for testing class `Promise`
class PromiseTest : public ::testing::Test
{};

// Tests default construction of an `int` promise
TEST_F(PromiseTest, DefaultIntConstruction)
{
    hycast::Promise<int> promise{};
    EXPECT_FALSE(promise.isDone());
    EXPECT_FALSE(promise.wasCanceled());
}

// Tests default construction of a `void` promise
TEST_F(PromiseTest, DefaultVoidConstruction)
{
    hycast::Promise<void> promise{};
    EXPECT_FALSE(promise.isDone());
    EXPECT_FALSE(promise.wasCanceled());
}

// Tests setting an exception in an `int` promise
TEST_F(PromiseTest, IntException)
{
    hycast::Promise<int> promise{};
    try {
        throw std::runtime_error("Dummy");
    }
    catch (const std::exception& e){
        promise.setException();
    }
    EXPECT_TRUE(promise.isDone());
    EXPECT_FALSE(promise.wasCanceled());
    EXPECT_THROW(promise.getResult(), std::runtime_error);
}

// Tests setting an exception in a `void` promise
TEST_F(PromiseTest, VoidException)
{
    hycast::Promise<void> promise{};
    try {
        throw std::runtime_error("Dummy");
    }
    catch (const std::exception& e){
        promise.setException();
    }
    EXPECT_TRUE(promise.isDone());
    EXPECT_FALSE(promise.wasCanceled());
    EXPECT_THROW(promise.getResult(), std::runtime_error);
}

// Tests canceling an `int` promise
TEST_F(PromiseTest, CancelInt)
{
    hycast::Promise<int> promise{};
    promise.markCanceled();
    EXPECT_TRUE(promise.isDone());
    EXPECT_TRUE(promise.wasCanceled());
    EXPECT_THROW(promise.getResult(), hycast::LogicError);
}

// Tests canceling a `void` promise
TEST_F(PromiseTest, CancelVoid)
{
    hycast::Promise<void> promise{};
    promise.markCanceled();
    EXPECT_TRUE(promise.isDone());
    EXPECT_TRUE(promise.wasCanceled());
    EXPECT_THROW(promise.getResult(), hycast::LogicError);
}

// Tests setting the result of an `int` promise
TEST_F(PromiseTest, IntResult)
{
    int result = 1;
    hycast::Promise<int> promise{};
    promise.setResult(result);
    EXPECT_TRUE(promise.isDone());
    EXPECT_FALSE(promise.wasCanceled());
    EXPECT_EQ(result, promise.getResult());
}

// Tests setting the result of a `void` promise
TEST_F(PromiseTest, VoidResult)
{
    hycast::Promise<void> promise{};
    promise.setResult();
    EXPECT_TRUE(promise.isDone());
    EXPECT_FALSE(promise.wasCanceled());
    promise.getResult();
}

// Tests waiting for an `int` promise
TEST_F(PromiseTest, WaitForIntResult)
{
    int result = 1;
    hycast::Promise<int> promise{};
    std::thread thread{[&promise,&result]{promise.setResult(result);}};
    promise.wait();
    EXPECT_TRUE(promise.isDone());
    EXPECT_FALSE(promise.wasCanceled());
    EXPECT_EQ(result, promise.getResult());
    thread.join();
}

// Tests waiting for a `void` promise
TEST_F(PromiseTest, WaitForVoidResult)
{
    hycast::Promise<void> promise{};
    std::thread thread{[&promise]{promise.setResult();}};
    promise.wait();
    EXPECT_TRUE(promise.isDone());
    EXPECT_FALSE(promise.wasCanceled());
    promise.getResult();
    thread.join();
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
