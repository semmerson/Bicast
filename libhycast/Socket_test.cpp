/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYRIGHT in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Socket_test.cpp
 * @author: Steven R. Emmerson
 *
 * This file tests class `Socket`.
 */

#include "Socket.h"

#include <gtest/gtest.h>
#include <fcntl.h>
#include <sys/stat.h>

namespace {

// The fixture for testing class Socket.
class SocketTest : public ::testing::Test {
 protected:
  // You can remove any or all of the following functions if its body
  // is empty.

  SocketTest() {
    // You can do set-up work for each test here.
      sock1 = open("/dev/null", 0);
      sock2 = open("/dev/null", 0);
  }

  virtual ~SocketTest() {
    // You can do clean-up work that doesn't throw exceptions here.
      close(sock1);
      close(sock2);
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

  // Objects declared here can be used by all tests in the test case for Socket.
  int sock1, sock2;

    bool is_open(int sock)
    {
        struct stat statbuf;
        return fstat(sock, &statbuf) == 0;
    }
};

// Tests invalid argument
TEST_F(SocketTest, InvalidArgument) {
    EXPECT_THROW(hycast::Socket s(-1), std::invalid_argument);
}

// Tests destruction
TEST_F(SocketTest, CloseOnDestruction) {
    hycast::Socket s(sock1);
    EXPECT_EQ(true, is_open(sock1));
    s.~Socket();
    EXPECT_EQ(false, is_open(sock1));
}

// Tests copy-construction
TEST_F(SocketTest, CopyConstruction) {
    hycast::Socket s1(sock1);
    EXPECT_EQ(true, is_open(sock1));
    hycast::Socket s2(s1);
    s1.~Socket();
    EXPECT_EQ(true, is_open(sock1));
    s2.~Socket();
    EXPECT_EQ(false, is_open(sock1));
}

// Tests move-assignment
TEST_F(SocketTest, MoveAssignment) {
    hycast::Socket s1 = hycast::Socket(sock1);
    EXPECT_EQ(true, is_open(sock1));
    s1.~Socket();
    EXPECT_EQ(false, is_open(sock1));
}

// Tests copy-assignment to empty instance
TEST_F(SocketTest, CopyAssignmentToEmpty) {
    hycast::Socket s1(sock1);
    EXPECT_EQ(true, is_open(sock1));
    hycast::Socket s2;
    s2 = s1;
    EXPECT_EQ(true, is_open(sock1));
    s1.~Socket();
    EXPECT_EQ(true, is_open(sock1));
    s2.~Socket();
    EXPECT_EQ(false, is_open(sock1));
}

// Tests copy-assignment to non-empty instance
TEST_F(SocketTest, CopyAssignmentToNonEmpty) {
    hycast::Socket s1(sock1);
    EXPECT_EQ(true, is_open(sock1));
    hycast::Socket s2(sock2);
    EXPECT_EQ(true, is_open(sock2));
    s2 = s1;
    EXPECT_EQ(true, is_open(sock1));
    EXPECT_EQ(false, is_open(sock2));
    s1.~Socket();
    EXPECT_EQ(true, is_open(sock1));
    s2.~Socket();
    EXPECT_EQ(false, is_open(sock1));
}

// Tests copy-assignment to self
TEST_F(SocketTest, CopyAssignmentToSelf) {
    hycast::Socket s1(sock1);
    EXPECT_EQ(true, is_open(sock1));
    s1 = s1;
    EXPECT_EQ(true, is_open(sock1));
}

// Tests equality operator
TEST_F(SocketTest, EqualityOperator) {
    hycast::Socket s1(sock1);
    EXPECT_EQ(true, s1 == s1);
    hycast::Socket s2(sock2);
    EXPECT_EQ(false, s2 == s1);
}

// Tests to_string()
TEST_F(SocketTest, ToString) {
    hycast::Socket s1(sock1);
    EXPECT_EQ(true, s1.to_string() == std::to_string(sock1));
    hycast::Socket s2(sock2);
    EXPECT_EQ(true, s1.to_string() != s2.to_string());
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
