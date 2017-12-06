/**
 * This file tests IPv4 support in class `InetAddr`.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYRIGHT in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Ipv4Addr_test.cpp
 * @author: Steven R. Emmerson
 */

#include "InetAddr.h"

#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>
#include <gtest/gtest.h>

namespace {

// The fixture for testing class Foo.
class Ipv4AddrTest : public ::testing::Test {
 protected:
  // You can remove any or all of the following functions if its body
  // is empty.

  Ipv4AddrTest() {
    // You can do set-up work for each test here.
  }

  virtual ~Ipv4AddrTest() {
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

static const char* ADDR1 = "128.117.140.56";
static const char* ADDR2 = "128.117.140.57";

// Tests construction from a struct in_addr
TEST_F(Ipv4AddrTest, in_addr_Construction) {
    struct in_addr inAddr;
    inAddr.s_addr = inet_addr(ADDR1);
    EXPECT_STREQ(ADDR1, hycast::InetAddr(inAddr).to_string().data());
}

// Tests copy construction
TEST_F(Ipv4AddrTest, CopyConstruction) {
    struct in_addr inAddr;
    inAddr.s_addr = inet_addr(ADDR1);
    hycast::InetAddr a1{inAddr};
    hycast::InetAddr a2{a1};
    EXPECT_STREQ(ADDR1, a2.to_string().data());
}

// Tests copy assignment
TEST_F(Ipv4AddrTest, CopyAssignment) {
    struct in_addr inAddr;
    inAddr.s_addr = inet_addr(ADDR1);
    hycast::InetAddr a1{inAddr};
    inAddr.s_addr = inet_addr(ADDR1);
    hycast::InetAddr a2{inAddr};
    a2 = a1;
    EXPECT_STREQ(ADDR1, a2.to_string().data());
    inAddr.s_addr = inet_addr(ADDR1);
    hycast::InetAddr a3{inAddr};
    a1 = a3;
    EXPECT_STREQ(ADDR1, a2.to_string().data());
}

#if 0
// Tests hash()
TEST_F(Ipv4AddrTest, hash) {
    hycast::InetAddr inAddr1{ADDR1};
    hycast::InetAddr inAddr2{ADDR2};
    EXPECT_TRUE(inAddr1.hash() != inAddr2.hash());
}
#endif

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
