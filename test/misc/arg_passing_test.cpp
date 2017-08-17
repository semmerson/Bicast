/**
 * This file tests argument passing in C++.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: arg_passing_test.cpp
 * Created On: Aug 17, 2017
 *     Author: Steven R. Emmerson
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
