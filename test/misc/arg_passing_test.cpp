/**
 * This file tests argument passing in C++.
 *
 *       File: arg_passing_test.cpp
 * Created On: Aug 17, 2017
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

#include "gtest/gtest.h"

namespace {

/// The fixture for testing class `Foo`
class ArgPass : public ::testing::Test
{
protected:
    void* objAsRef(ArgPass& obj)
    {
        return &obj;
    }
};

// Tests passing object as reference
TEST_F(ArgPass, ObjAsRef)
{
    EXPECT_EQ(this, objAsRef(*this));
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
