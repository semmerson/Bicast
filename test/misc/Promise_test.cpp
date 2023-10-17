/**
 * This file tests class `Promise`.
 *
 *       File: Promise_test.cpp
 * Created On: Jun 5, 2017
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
#include "Promise.h"

#include "gtest/gtest.h"
#include <thread>

namespace {

using namespace bicast;

/// The fixture for testing class `Promise`
class PromiseTest : public ::testing::Test
{};

// Tests default construction of an `int` promise
TEST_F(PromiseTest, DefaultIntConstruction)
{
    Promise<int> promise{};
    EXPECT_FALSE(promise.isDone());
    EXPECT_FALSE(promise.wasCanceled());
}

// Tests default construction of a `void` promise
TEST_F(PromiseTest, DefaultVoidConstruction)
{
    Promise<void> promise{};
    EXPECT_FALSE(promise.isDone());
    EXPECT_FALSE(promise.wasCanceled());
}

// Tests setting an exception in an `int` promise
TEST_F(PromiseTest, IntException)
{
    Promise<int> promise{};
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
    Promise<void> promise{};
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
    Promise<int> promise{};
    promise.markCanceled();
    EXPECT_TRUE(promise.isDone());
    EXPECT_TRUE(promise.wasCanceled());
    EXPECT_THROW(promise.getResult(), LogicError);
}

// Tests canceling a `void` promise
TEST_F(PromiseTest, CancelVoid)
{
    Promise<void> promise{};
    promise.markCanceled();
    EXPECT_TRUE(promise.isDone());
    EXPECT_TRUE(promise.wasCanceled());
    EXPECT_THROW(promise.getResult(), LogicError);
}

// Tests setting the result of an `int` promise
TEST_F(PromiseTest, IntResult)
{
    int result = 1;
    Promise<int> promise{};
    promise.setResult(result);
    EXPECT_TRUE(promise.isDone());
    EXPECT_FALSE(promise.wasCanceled());
    EXPECT_EQ(result, promise.getResult());
}

// Tests setting the result of a `void` promise
TEST_F(PromiseTest, VoidResult)
{
    Promise<void> promise{};
    promise.setResult();
    EXPECT_TRUE(promise.isDone());
    EXPECT_FALSE(promise.wasCanceled());
    promise.getResult();
}

// Tests waiting for an `int` promise
TEST_F(PromiseTest, WaitForIntResult)
{
    int result = 1;
    Promise<int> promise{};
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
    Promise<void> promise{};
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
