/**
 * This file tests the McastSender class.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: McastSender_test.cpp
 * @author: Steven R. Emmerson
 */

#include "McastSender.h"

#include <cstring>
#include <gtest/gtest.h>

namespace {

// The fixture for testing class McastSender.
class McastSenderTest : public ::testing::Test {
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    McastSenderTest() {
        // You can do set-up work for each test here.
    }

    virtual ~McastSenderTest() {
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

    // Objects declared here can be used by all tests in the test case for McastSender.
};

// Tests construction
TEST_F(McastSenderTest, Construction) {
    hycast::InetSockAddr mcastAddr("localhost", 38800);
    hycast::McastSender(mcastAddr, 0);
}

// Tests sending a data-product
TEST_F(McastSenderTest, SendDataProduct) {
    hycast::InetSockAddr    mcastAddr("234.128.117.0", 38800);
    hycast::McastSender     sender(mcastAddr, 0);
    const std::string       prodName("product");
    const hycast::ProdIndex prodIndex(0);
    const hycast::ProdSize  prodSize{38000};
    const hycast::ChunkSize chunkSize{1000};
    hycast::ProdInfo        prodInfo(prodName, prodIndex, prodSize, chunkSize);
    hycast::Product         prod(prodInfo);
    hycast::ChunkIndex      chunkIndex = 0;
    for (size_t remaining = prodSize; remaining > 0; ++chunkIndex) {
        const size_t        dataSize =
                remaining < chunkSize ? remaining : chunkSize;
        char                data[dataSize];
        ::memset(data, 0xbd, dataSize);
        hycast::ChunkInfo   chunkInfo(prodIndex, chunkIndex);
        hycast::ActualChunk chunk(chunkInfo, data, dataSize);
        prod.add(chunk);
        remaining -= dataSize;
    }
    sender.send(prod);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
