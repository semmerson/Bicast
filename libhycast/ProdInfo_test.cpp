/**
 * This file tests the class `ProdInfo`.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ProdInfo_test.cpp
 * @author: Steven R. Emmerson
 */


#include "ProdInfo.h"
#include "ClientSocket.h"
#include "InetSockAddr.h"
#include "ServerSocket.h"
#include "Socket.h"

#include <arpa/inet.h>
#include <cstring>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sstream>
#include <thread>

namespace {

// The fixture for testing class ProdInfo.
class ProdInfoTest : public ::testing::Test {
 protected:
  // You can remove any or all of the following functions if its body
  // is empty.

  ProdInfoTest() {
    // You can do set-up work for each test here.
  }

  virtual ~ProdInfoTest() {
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
};

// Tests default construction
TEST_F(ProdInfoTest, DefaultConstruction) {
    hycast::ProdInfo info;
    EXPECT_STREQ("", info.getName().data());
    EXPECT_EQ(0, info.getIndex());
    EXPECT_EQ(0, info.getSize());
    EXPECT_EQ(0, info.getChunkSize());
}

// Tests construction
TEST_F(ProdInfoTest, Construction) {
    hycast::ProdInfo info("name", 1, 2, 3);
    EXPECT_STREQ("name", info.getName().data());
    EXPECT_EQ(1, info.getIndex());
    EXPECT_EQ(2, info.getSize());
    EXPECT_EQ(3, info.getChunkSize());
}

// Tests equals()
TEST_F(ProdInfoTest, Equals) {
    hycast::ProdInfo info1("name", 1, 2, 3);
    EXPECT_TRUE(info1.equals(info1));
    hycast::ProdInfo info2("name", 1, 2, 2);
    EXPECT_FALSE(info1.equals(info2));
    hycast::ProdInfo info3("name", 1, 1, 3);
    EXPECT_FALSE(info1.equals(info3));
    hycast::ProdInfo info4("name", 2, 2, 3);
    EXPECT_FALSE(info1.equals(info4));
    hycast::ProdInfo info5("names", 1, 2, 3);
    EXPECT_FALSE(info1.equals(info5));
}

// Tests getSerialSize()
TEST_F(ProdInfoTest, GetSerialSize) {
    hycast::ProdInfo info1("name", 1, 2, 3);
    EXPECT_EQ(16, info1.getSerialSize(0));
}

// Tests serialization/de-serialization
TEST_F(ProdInfoTest, Serialization) {
    hycast::ProdInfo info1("name", 1, 2, 3);
    const size_t nbytes = info1.getSerialSize(0);
    alignas(alignof(size_t)) char bytes[nbytes];
    info1.serialize(bytes, nbytes, 0);
    hycast::ProdInfo info2(bytes, nbytes, 0);
    EXPECT_TRUE(info1.equals(info2));
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
