/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYRIGHT in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Inet4Addr_test.cpp
 * @author: Steven R. Emmerson
 *
 * This file tests class `Inet4Addr`.
 */


#include "Inet4Addr.h"

#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>
#include <gtest/gtest.h>

namespace {

// The fixture for testing class Foo.
class Inet4AddrTest : public ::testing::Test {
 protected:
  // You can remove any or all of the following functions if its body
  // is empty.

  Inet4AddrTest() {
    // You can do set-up work for each test here.
  }

  virtual ~Inet4AddrTest() {
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

// Tests construction from an in_addr_t
TEST_F(Inet4AddrTest, InAddrTConstruction) {
  EXPECT_STREQ(ADDR1,
          hycast::Inet4Addr(inet_addr(ADDR1)).to_string().data());
}

// Tests construction from a struct in_addr
TEST_F(Inet4AddrTest, in_addr_Construction) {
    struct in_addr inAddr;
    inAddr.s_addr = inet_addr(ADDR1);
    EXPECT_STREQ(ADDR1, hycast::Inet4Addr(inAddr).to_string().data());
}

// Tests copy construction
TEST_F(Inet4AddrTest, CopyConstruction) {
    hycast::Inet4Addr a1{inet_addr(ADDR1)};
    hycast::Inet4Addr a2{a1};
    EXPECT_STREQ(ADDR1, a2.to_string().data());
}

// Tests copy assignment
TEST_F(Inet4AddrTest, CopyAssignment) {
    hycast::Inet4Addr a1{inet_addr(ADDR1)};
    hycast::Inet4Addr a2{inet_addr(ADDR2)};
    a2 = a1;
    EXPECT_STREQ(ADDR1, a2.to_string().data());
    hycast::Inet4Addr a3{inet_addr(ADDR1)};
    a1 = a3;
    EXPECT_STREQ(ADDR1, a2.to_string().data());
}

#if 0
// Tests hash()
TEST_F(Inet4AddrTest, hash) {
    hycast::Inet4Addr inAddr1{ADDR1};
    hycast::Inet4Addr inAddr2{ADDR2};
    EXPECT_TRUE(inAddr1.hash() != inAddr2.hash());
}
#endif

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
