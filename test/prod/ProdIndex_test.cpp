/**
 * This file tests the class `ProdIndex`
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ProdIndex_test.cpp
 * @author: Steven R. Emmerson
 */

#include "Codec.h"
#include "InetSockAddr.h"
#include "ProdIndex.h"
#include "SctpSock.h"
#include "SctpSock.h"

#include <arpa/inet.h>
#include <cstring>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sstream>
#include <thread>

namespace {

// The fixture for testing class ProdIndex.
class ProdIndexTest : public ::testing::Test {
 protected:
};

// Tests default construction
TEST_F(ProdIndexTest, DefaultConstruction) {
    hycast::ProdIndex index;
    EXPECT_EQ(0, (uint32_t)index);
}

// Tests construction
TEST_F(ProdIndexTest, Construction) {
    hycast::ProdIndex index(1);
    EXPECT_EQ(1, (uint32_t)index);
}

// Tests comparison
TEST_F(ProdIndexTest, Comparison) {
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

    hycast::ProdIndex index3(hycast::ProdIndex::prodIndexMax-1);
    hycast::ProdIndex index4(hycast::ProdIndex::prodIndexMax);
    EXPECT_TRUE(index3 < index4);
    EXPECT_TRUE(index3 <= index4);
    EXPECT_TRUE(index3 <= index3);
    EXPECT_TRUE(index4 > index3);
    EXPECT_TRUE(index4 >= index3);
    EXPECT_TRUE(index4 >= index4);
    EXPECT_TRUE(index4 < index1);
    EXPECT_FALSE(index1 < index4);
    EXPECT_TRUE(index4.isEarlierThan(index1));
    EXPECT_FALSE(index1.isEarlierThan(index4));
}

// Tests increment
TEST_F(ProdIndexTest, Increment) {
    hycast::ProdIndex index(0);
    EXPECT_EQ(1, ++index);
}

// Tests decrement
TEST_F(ProdIndexTest, Decrement) {
    hycast::ProdIndex index(1);
    EXPECT_EQ(0, --index);
}

// Tests getSerialSize()
TEST_F(ProdIndexTest, GetSerialSize) {
    hycast::ProdIndex index(1);
    EXPECT_EQ(sizeof(hycast::ProdIndex::type), index.getSerialSize(0));
}

// Tests serialization/de-serialization
TEST_F(ProdIndexTest, Serialization) {
    hycast::ProdIndex index1(1);
    const size_t nbytes = index1.getSerialSize(0);
    alignas(alignof(size_t)) char bytes[nbytes];
    hycast::MemEncoder encoder(bytes, nbytes);
    index1.serialize(encoder, 0);
    encoder.flush();
    hycast::MemDecoder decoder(bytes, nbytes);
    decoder.fill(0);
    auto index2 = hycast::ProdIndex::deserialize(decoder, 0);
    EXPECT_TRUE(index1 == index2);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
