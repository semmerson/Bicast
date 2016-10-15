/**
 * This file tests the class `PeerConnection`.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerConnection_test.cpp
 * @author: Steven R. Emmerson
 */


#include <gtest/gtest.h>
#include "PeerConnection.h"

namespace {

// The fixture for testing class PeerConnection.
class PeerConnectionTest : public ::testing::Test {
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    PeerConnectionTest() {
        // You can do set-up work for each test here.
    }

    virtual ~PeerConnectionTest() {
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

    // Objects declared here can be used by all tests in the test case for PeerConnection.
};

// Tests Construction
TEST_F(PeerConnectionTest, Construction) {
    hycast::Socket         sock;
    hycast::PeerConnection conn{sock};
}

// Tests transmission
TEST_F(PeerConnectionTest, Transmission) {
    ASSERT_TRUE(false);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
