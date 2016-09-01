/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYRIGHT in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Di_test.cpp
 * @author: Steven R. Emmerson
 *
 * This file ...
 */


#include "Di.h"

#include <gtest/gtest.h>

namespace {

// The fixture for testing class Di.
class DiTest : public ::testing::Test {
 protected:
  // You can remove any or all of the following functions if its body
  // is empty.

  DiTest() {
    // You can do set-up work for each test here.
  }

  virtual ~DiTest() {
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

  // Objects declared here can be used by all tests in the test case for Di.
};

// Tests construction
TEST_F(DiTest, Construction) {
  Di di1(1);
  EXPECT_EQ(1, di1.get());
  Di di2(2);
  EXPECT_EQ(2, di2.get());
}

// Tests copy assignment
TEST_F(DiTest, CopyAssignment) {
  Di di1(1);
  Di di2(2);
  di2 = di1;
  EXPECT_EQ(1, di2.get());
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
