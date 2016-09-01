/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: InetAddr_test.cpp
 * @author: Steven R. Emmerson
 *
 * This file tests class `InetAddr`.
 */

#include "InetAddr.h"

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <string.h>
#include <sys/socket.h>

namespace {

// The fixture for testing class InetAddr.
class InetAddrTest : public ::testing::Test {
 protected:
  // You can remove any or all of the following functions if its body
  // is empty.

  InetAddrTest()
     : ipv4Addr("128.117.140.56"),
       ipv6Addr("2001:db8::ff00:42:8329")
  {
  }

  virtual ~InetAddrTest() {
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

  // Objects declared here can be used by all tests in the test case for
  // SockAddrInet.
  hycast::InetAddr ipv4Addr;
  hycast::InetAddr ipv6Addr;
};

static const char* IPV4_ADDR0 = "0.0.0.0";
static const char* IPV4_ADDR1 = "128.117.140.56";
static const char* IPV4_ADDR2 = "128.117.140.57";

static const char* IPV6_ADDR1 = "2001:db8::ff00:42:8329";
static const char* IPV6_ADDR2 = "2001:db8::ff00:42:8330";

// Tests default construction
TEST_F(InetAddrTest, DefaultConstruction) {
    hycast::InetAddr addr;
    EXPECT_STREQ(IPV4_ADDR0, addr.to_string().data());
}

// Tests construction from IP address string
TEST_F(InetAddrTest, StringIpAndConstruction) {
    hycast::InetAddr addr1{IPV4_ADDR1};
    EXPECT_STREQ(IPV4_ADDR1, addr1.to_string().data());
    hycast::InetAddr addr2{IPV6_ADDR1};
    EXPECT_STREQ(IPV6_ADDR1, addr2.to_string().data());
}

// Tests construction from an in_addr_t
TEST_F(InetAddrTest, InAddrTAndConstruction) {
    hycast::InetAddr addr{inet_addr(IPV4_ADDR1)};
    EXPECT_STREQ(IPV4_ADDR1, addr.to_string().data());
}

// Tests construction from a struct in_addr
TEST_F(InetAddrTest, StructAddrInConstruction) {
    struct in_addr ipv4Addr;
    EXPECT_EQ(1, inet_pton(AF_INET, IPV4_ADDR1, &ipv4Addr));
    hycast::InetAddr addr{ipv4Addr};
    EXPECT_STREQ(IPV4_ADDR1, addr.to_string().data());
}

// Tests construction from IPv6 address
TEST_F(InetAddrTest, Ipv6AddrAndConstruction) {
    struct in6_addr ipv6Addr;
    EXPECT_EQ(1, inet_pton(AF_INET6, IPV6_ADDR1, &ipv6Addr));
    hycast::InetAddr inetAddr{ipv6Addr};
    EXPECT_STREQ(IPV6_ADDR1, inetAddr.to_string().data());
}

// Tests copy construction
TEST_F(InetAddrTest, CopyConstruction) {
    hycast::InetAddr addr1{IPV4_ADDR1};
    hycast::InetAddr addr2{addr1};
    EXPECT_STREQ(IPV4_ADDR1, addr2.to_string().data());
}

// Tests copy assignment
TEST_F(InetAddrTest, CopyAssignment) {
    hycast::InetAddr addr1{IPV4_ADDR1};
    hycast::InetAddr addr2{};
    addr2 = addr1;
    EXPECT_STREQ(IPV4_ADDR1, addr2.to_string().data());
    hycast::InetAddr addr3{IPV4_ADDR2};
    addr1 = addr3;
    EXPECT_STREQ(IPV4_ADDR1, addr2.to_string().data());
}

// Tests hash
TEST_F(InetAddrTest, Hash) {
    EXPECT_EQ(true, hycast::InetAddr{IPV4_ADDR1}.hash() ==
            hycast::InetAddr{IPV4_ADDR1}.hash());
    EXPECT_EQ(true, hycast::InetAddr{IPV6_ADDR1}.hash() ==
            hycast::InetAddr{IPV6_ADDR1}.hash());
    EXPECT_EQ(true, hycast::InetAddr{IPV4_ADDR1}.hash() !=
            hycast::InetAddr{IPV6_ADDR1}.hash());
}

// Tests compare
TEST_F(InetAddrTest, Compare) {
    EXPECT_EQ(0, hycast::InetAddr{IPV4_ADDR1}.compare(
            hycast::InetAddr{IPV4_ADDR1}));
    EXPECT_EQ(true, hycast::InetAddr{IPV4_ADDR1}.compare(
            hycast::InetAddr{IPV4_ADDR2}) < 0);

    EXPECT_EQ(0, hycast::InetAddr{IPV6_ADDR1}.compare(
            hycast::InetAddr{IPV6_ADDR1}));
    EXPECT_EQ(true, hycast::InetAddr{IPV6_ADDR1}.compare(
            hycast::InetAddr{IPV6_ADDR2}) < 0);

    EXPECT_EQ(true, hycast::InetAddr{IPV4_ADDR1}.compare(
            hycast::InetAddr{IPV6_ADDR1}) < 0);
}

// Tests equals
TEST_F(InetAddrTest, Equals) {
    hycast::InetAddr ipv4Addr1 = hycast::InetAddr(IPV4_ADDR1);
    EXPECT_TRUE(ipv4Addr1.equals(ipv4Addr1));

    hycast::InetAddr ipv4Addr2 = hycast::InetAddr(IPV4_ADDR2);
    EXPECT_FALSE(ipv4Addr1.equals(ipv4Addr2));
    EXPECT_FALSE(ipv4Addr2.equals(ipv4Addr1));

    hycast::InetAddr ipv6Addr1 = hycast::InetAddr(IPV6_ADDR1);
    EXPECT_TRUE(ipv6Addr1.equals(ipv6Addr1));

    hycast::InetAddr ipv6Addr2 = hycast::InetAddr(IPV6_ADDR2);
    EXPECT_FALSE(ipv6Addr1.equals(ipv6Addr2));
    EXPECT_FALSE(ipv6Addr2.equals(ipv6Addr1));

    EXPECT_FALSE(ipv4Addr1.equals(ipv6Addr1));
    EXPECT_FALSE(ipv6Addr2.equals(ipv4Addr1));
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
