/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYRIGHT in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ServerPeerConnection_test.cpp
 * @author: Steven R. Emmerson
 *
 * This file tests class `ServerPeerConnection`.
 */


#include "Socket.h"

#include <gtest/gtest.h>
#include <ServerPeerConnection.h>
#include <stdexcept>

namespace {

// The fixture for testing class Foo.
class ServerPeerConnectionTest : public ::testing::Test {
 protected:
  // You can remove any or all of the following functions if its body
  // is empty.

  ServerPeerConnectionTest() {
    // You can do set-up work for each test here.
  }

  virtual ~ServerPeerConnectionTest() {
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
};

// Tests construction
TEST_F(ServerPeerConnectionTest, Construction) {
    hycast::ServerPeerConnection connection;
    EXPECT_EQ(0, connection.getNumSockets());
}

// Tests copy construction
TEST_F(ServerPeerConnectionTest, CopyConstruction) {
    hycast::ServerPeerConnection conn1;
    hycast::ServerPeerConnection conn2(conn1);
    EXPECT_EQ(0, conn2.getNumSockets());
}

// Tests adding a socket
TEST_F(ServerPeerConnectionTest, add_socket) {
    hycast::ServerPeerConnection connection;
    hycast::Socket s = hycast::Socket(3);
    EXPECT_EQ(false, connection.add_socket(s));
    s = hycast::Socket(4);
    EXPECT_EQ(false, connection.add_socket(s));
    s = hycast::Socket(5);
    EXPECT_EQ(true, connection.add_socket(s));
    s = hycast::Socket(6);
    EXPECT_THROW(connection.add_socket(s), std::length_error);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
