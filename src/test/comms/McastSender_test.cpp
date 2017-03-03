/**
 * This file tests the McastSender class.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: McastSender_test.cpp
 * @author: Steven R. Emmerson
 */

#include "McastSender.h"

#include <gtest/gtest.h>

namespace {

// The fixture for testing class McastSender.
class McastSenderTest : public ::testing::Test {
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    McastSenderTest() {
        // You can do set-up work for each test here.
    }

    virtual ~McastSenderTest() {
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

    // Objects declared here can be used by all tests in the test case for McastSender.
};

// Tests construction
TEST_F(McastSenderTest, Construction) {
    hycast::InetSockAddr mcastAddr("localhost", 38800);
    hycast::McastSender(mcastAddr, 0);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
