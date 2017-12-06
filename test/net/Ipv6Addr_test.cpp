/**
 * This file tests IPv6 support in class `InetAddr`
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
class Ipv6AddrTest : public ::testing::Test {
 protected:
  // You can remove any or all of the following functions if its body
  // is empty.

  Ipv6AddrTest() {
    // You can do set-up work for each test here.
  }

  virtual ~Ipv6AddrTest() {
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

static const char* ADDR1 = "2001:db8::ff00:42:8329";
static const char* ADDR2 = "2001:db8::ff00:42:8330";

// Tests construction from a struct in6_addr
TEST_F(Ipv6AddrTest, in_addr_Construction) {
    struct in6_addr inAddr;
    EXPECT_EQ(1, inet_pton(AF_INET6, ADDR1, &inAddr.s6_addr));
    EXPECT_STREQ(ADDR1, hycast::InetAddr(inAddr).to_string().data());
}

// Tests copy construction
TEST_F(Ipv6AddrTest, CopyConstruction) {
    struct in6_addr inAddr;
    EXPECT_EQ(1, inet_pton(AF_INET6, ADDR1, &inAddr.s6_addr));
    hycast::InetAddr a1{inAddr};
    hycast::InetAddr a2{a1};
    EXPECT_STREQ(ADDR1, a2.to_string().data());
}

// Tests copy assignment
TEST_F(Ipv6AddrTest, CopyAssignment) {
    struct in6_addr inAddr;
    EXPECT_EQ(1, inet_pton(AF_INET6, ADDR1, &inAddr.s6_addr));
    hycast::InetAddr a1{inAddr};
    EXPECT_EQ(1, inet_pton(AF_INET6, ADDR2, &inAddr.s6_addr));
    hycast::InetAddr a2{inAddr};;
    EXPECT_STREQ(ADDR2, a2.to_string().data());
    a2 = a1;
    EXPECT_STREQ(ADDR1, a2.to_string().data());
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
