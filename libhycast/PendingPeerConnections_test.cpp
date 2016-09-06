/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PendingPeerConnections_test.cpp
 * @author: Steven R. Emmerson
 *
 * This file tests class `PendingPeerConnections`.
 */

#include "PendingPeerConnections.h"

#include <arpa/inet.h>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>

namespace {

// The fixture for testing class PendingPeerConnections.
class PendingPeerConnectionsTest : public ::testing::Test {
 protected:
  // You can remove any or all of the following functions if its body
  // is empty.

  PendingPeerConnectionsTest() {
    // You can do set-up work for each test here.
  }

  virtual ~PendingPeerConnectionsTest() {
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

  /*
   * Objects declared here can be used by all tests in the test case for
   * PendingPeerConnections.
   */
};

// Tests invalid construction
TEST_F(PendingPeerConnectionsTest, InvalidConstruction) {
  EXPECT_THROW(hycast::PendingPeerConnections(0), std::invalid_argument);
}

// Tests default construction
TEST_F(PendingPeerConnectionsTest, Constructor) {
  hycast::PendingPeerConnections pending;
  EXPECT_EQ(0, pending.numPending());
}

// Tests adding an invalid socket
TEST_F(PendingPeerConnectionsTest, AddInvalidSocket) {
    uint8_t bytes[] = {1};
    hycast::PeerId peerId1{bytes, sizeof(bytes)};
    hycast::PendingPeerConnections pending(1);
    std::shared_ptr<hycast::PeerConnection> conn =
            pending.addSocket(peerId1, hycast::Socket(3));
    conn = pending.addSocket(peerId1, hycast::Socket(4));
    EXPECT_THROW(pending.addSocket(peerId1, hycast::Socket(3)),
            std::invalid_argument);
    EXPECT_EQ(1, pending.numPending());
}

// Tests adding sockets
TEST_F(PendingPeerConnectionsTest, AddSockets) {
    uint8_t bytes[] = {1};
    hycast::PeerId peerId1{bytes, sizeof(bytes)};
    hycast::PendingPeerConnections pending(1);
    EXPECT_EQ(0, pending.numPending());
    std::shared_ptr<hycast::PeerConnection> conn =
            pending.addSocket(peerId1, hycast::Socket(3));
    EXPECT_EQ(nullptr, conn.get());
    EXPECT_EQ(1, pending.numPending());
    conn = pending.addSocket(peerId1, hycast::Socket(4));
    EXPECT_EQ(nullptr, conn.get());
    EXPECT_EQ(1, pending.numPending());
    conn = pending.addSocket(peerId1, hycast::Socket(5));
    EXPECT_NE((void*)0, conn.get());
    EXPECT_EQ(0, pending.numPending());

    bytes[0] = 2;
    hycast::PeerId peerId2(bytes, sizeof(bytes));
    bytes[0] = 3;
    hycast::PeerId peerId3(bytes, sizeof(bytes));

    conn = pending.addSocket(peerId2, hycast::Socket(6));
    EXPECT_EQ(nullptr, conn.get());
    conn = pending.addSocket(peerId2, hycast::Socket(7));
    EXPECT_EQ(nullptr, conn.get());
    conn = pending.addSocket(peerId3, hycast::Socket(8));
    EXPECT_EQ(nullptr, conn.get());
    EXPECT_EQ(1, pending.numPending());
    conn = pending.addSocket(peerId2, hycast::Socket(9));
    EXPECT_EQ(nullptr, conn.get());
    EXPECT_EQ(1, pending.numPending());
    conn = pending.addSocket(peerId3, hycast::Socket(10));
    EXPECT_EQ(nullptr, conn.get());
    conn = pending.addSocket(peerId3, hycast::Socket(11));
    EXPECT_EQ(nullptr, conn.get());
    conn = pending.addSocket(peerId3, hycast::Socket(12));
    EXPECT_NE(nullptr, conn.get());
    EXPECT_EQ(0, pending.numPending());
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
