/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerId_test.cpp
 * @author: Steven R. Emmerson
 *
 * This file tests class `PeerId`.
 */

#include "PeerId.h"

#include <gtest/gtest.h>

namespace {

// The fixture for testing class Foo.
class PeerIdTest : public ::testing::Test {
 protected:
  // You can remove any or all of the following functions if its body
  // is empty.

  PeerIdTest() {
    // You can do set-up work for each test here.
  }

  virtual ~PeerIdTest() {
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

  // Objects declared here can be used by all tests in the test case for Foo.
  const uint8_t peer1[1] {1};
  const uint8_t peer2[2] {2, 0};
  const hycast::PeerId peerId1{peer1, sizeof(peer1)};
  const hycast::PeerId peerId2{peer2, sizeof(peer2)};
};

// Tests construction with invalid peer identifier
TEST_F(PeerIdTest, InvalidConstruction) {
    EXPECT_THROW(hycast::PeerId(nullptr, 0), std::invalid_argument);
}

// Tests copy construction
TEST_F(PeerIdTest, CopyConstruction) {
    hycast::PeerId peerId(peerId1);
    EXPECT_STREQ("0x01", peerId.to_string().data());
}

// Tests copy assignment
TEST_F(PeerIdTest, CopyAssignment) {
    hycast::PeerId peerId(peerId1);
    peerId = peerId; // Self-assignment
    EXPECT_STREQ("0x01", peerId.to_string().data());
    peerId = peerId2;
    EXPECT_STREQ("0x0200", peerId.to_string().data());
}

// Tests to_string()
TEST_F(PeerIdTest, to_string) {
    EXPECT_STREQ("0x01", peerId1.to_string().data());
}

// Tests hash()
TEST_F(PeerIdTest, hash) {
    EXPECT_TRUE(peerId1.hash() == peerId1.hash());
    EXPECT_TRUE(peerId2.hash() == peerId2.hash());
    EXPECT_TRUE(peerId1.hash() != peerId2.hash());
}

// Tests compare()
TEST_F(PeerIdTest, compare) {
    EXPECT_EQ(0, peerId1.compare(peerId1));
    EXPECT_EQ(0, peerId2.compare(peerId2));
    EXPECT_TRUE(peerId1.compare(peerId2) < 0);
}

// Tests equals()
TEST_F(PeerIdTest, equals) {
    EXPECT_TRUE(peerId1.equals(peerId1));
    EXPECT_TRUE(peerId2.equals(peerId2));
    EXPECT_FALSE(peerId1.equals(peerId2));

    hycast::PeerId peerId(peerId1);
    EXPECT_TRUE(peerId.equals(peerId1));
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
