/**
 * This file tests RunPar
 *
 *       File: RunPar_test.cpp
 *     Author: Steven R. Emmerson
 *
 *    Copyright 2023 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License
 * is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied. See the License for the specific language governing permissions and limitations under
 * the License.
 */
#include "config.h"

#include "RunPar.h"

#include "gtest/gtest.h"

namespace {

using namespace bicast;

/// The fixture for testing `RunPar`
class RunParTest : public ::testing::Test
{
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    RunParTest()
    {
        // You can do set-up work for each test here.
    }

    virtual ~RunParTest()
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

// Tests construction
TEST_F(RunParTest, Construction)
{
    const char* argv[] = {__FILE__, NULL};
    RunPar::init(1, argv);
    EXPECT_STREQ(::basename(__FILE__), RunPar::progName.data());
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
