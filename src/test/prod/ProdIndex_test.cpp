/**
 * This file tests the class `ProdIndex`
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ProdInfo_test.cpp
 * @author: Steven R. Emmerson
 */

#include "ClntSctpSock.h"
#include "InetSockAddr.h"
#include "ProdIndex.h"
#include "SctpSock.h"
#include "SrvrSctpSock.h"

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
    hycast::ProdIndex index;
    EXPECT_EQ(0, (uint32_t)index);
}

// Tests construction
TEST_F(ProdInfoTest, Construction) {
    hycast::ProdIndex index(1);
    EXPECT_EQ(1, (uint32_t)index);
}

// Tests comparison
TEST_F(ProdInfoTest, Comparison) {
    hycast::ProdIndex index1(1);
    EXPECT_TRUE(index1 == index1);
    hycast::ProdIndex index2(2);
    EXPECT_FALSE(index1 == index2);
    EXPECT_TRUE(index1 != index2);
    EXPECT_TRUE(index1 <= index1);
    EXPECT_TRUE(index1 >= index1);
    EXPECT_TRUE(index1 < index2);
    EXPECT_TRUE(index1 <= index2);
    EXPECT_TRUE(index2 > index1);
    EXPECT_TRUE(index2 >= index1);
}

// Tests increment
TEST_F(ProdInfoTest, Increment) {
    hycast::ProdIndex index(0);
    EXPECT_EQ(1, ++index);
}

// Tests decrement
TEST_F(ProdInfoTest, Decrement) {
    hycast::ProdIndex index(1);
    EXPECT_EQ(0, --index);
}

// Tests getSerialSize()
TEST_F(ProdInfoTest, GetSerialSize) {
    hycast::ProdIndex index(1);
    EXPECT_EQ(4, index.getSerialSize(0));
}

// Tests serialization/de-serialization
TEST_F(ProdInfoTest, Serialization) {
    hycast::ProdIndex index1(1);
    const size_t nbytes = index1.getSerialSize(0);
    alignas(alignof(size_t)) char bytes[nbytes];
    index1.serialize(bytes, nbytes, 0);
    hycast::ProdIndex index2(bytes, nbytes, 0);
    EXPECT_TRUE(index1 == index2);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
