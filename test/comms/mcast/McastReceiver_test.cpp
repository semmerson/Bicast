/**
 * This file tests the `McastReceiver` class.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: McastReceiver_test.cpp
 * @author: Steven R. Emmerson
 */
#include "config.h"

#include "error.h"
#include "InetSockAddr.h"
#include "Interface.h"

#include <atomic>
#include <gtest/gtest.h>
#include <mcast/McastMsgRcvr.h>
#include <mcast/McastReceiver.h>
#include <mcast/McastSender.h>
#include <pthread.h>
#include <thread>
#include <unistd.h>

namespace {

// The fixture for testing class McastReceiver.
class McastReceiverTest : public ::testing::Test, public hycast::McastMsgRcvr {
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    McastReceiverTest()
        : asmGroupAddr("234.128.117.0", 38800)
        , ssmGroupAddr("232.0.0.0", 38800)
        //, srcAddr{"127.0.0.1"} // DOESN'T WORK
        , srcAddr{hycast::Interface{ETHNET_IFACE_NAME}.getInetAddr(AF_INET)}
        , version{0}
        , prodName("product")
        , chunkSize{hycast::ChunkInfo::getCanonSize()}
        , prodIndex(0)
        , prodSize{38000}
        , prodInfo(prodName, prodIndex, prodSize)
        , prod(prodInfo)
        , data{new char[chunkSize]}
        , datum{static_cast<char>(0xbd)}
        , chunkIndex{0}
        , numChunks{0}
    {
        ::memset(data, datum, chunkSize);
    }

    virtual ~McastReceiverTest() {
        // You can do clean-up work that doesn't throw exceptions here.
        delete[] data;
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

    void recvNotice(const hycast::ProdInfo& info)
    {
        EXPECT_TRUE(info == prodInfo);
        numChunks = info.getNumChunks();
    }

    void recvData(hycast::LatentChunk chunk)
    {
        EXPECT_EQ(prodIndex, chunk.getProdIndex());
        EXPECT_EQ(chunkIndex, chunk.getIndex());
        const hycast::ChunkSize expectedSize =
                prodInfo.getChunkSize(chunkIndex);
        char data[expectedSize];
        const size_t actualSize = chunk.drainData(data, sizeof(data));
        EXPECT_EQ(expectedSize, actualSize);
        for (unsigned i = 0; i < actualSize; ++i)
            ASSERT_EQ(datum, data[i]);
        ++chunkIndex;
    }

    void sendProduct(const hycast::InetSockAddr mcastAddr) {
        hycast::McastSender     sender(mcastAddr, version);
        hycast::ChunkIndex      numChunks = prodInfo.getNumChunks();
        for (hycast::ChunkIndex chunkIndex = 0; chunkIndex < numChunks;
                ++chunkIndex) {
            hycast::ChunkInfo   chunkInfo(prodInfo, chunkIndex);
            hycast::ActualChunk chunk(chunkInfo, data);
            prod.add(chunk);
        }
        sender.send(prod);
    }

    void runReceiver(hycast::McastReceiver& mcastRcvr)
    {
        try {
            mcastRcvr();
        }
        catch (std::exception& e) {
            hycast::log_error(e);
        }
    }

    // Objects declared here can be used by all tests in the test case for McastReceiver.
    hycast::InetSockAddr  asmGroupAddr; // Any-source multicast-group address
    hycast::InetSockAddr  ssmGroupAddr; // Source-specific multicast-group address
    hycast::InetAddr      srcAddr; // IP address of source
    unsigned              version;
    std::string           prodName;
    hycast::ChunkSize     chunkSize;
    hycast::ProdIndex     prodIndex;
    hycast::ProdSize      prodSize;
    hycast::ProdInfo      prodInfo;
    hycast::Product       prod;
    char*                 data;
    char                  datum;
    volatile hycast::ChunkIndex    chunkIndex;
    volatile hycast::ChunkIndex    numChunks;
};

// Tests construction of any-source multicast receiver
TEST_F(McastReceiverTest, Construction) {
    hycast::McastReceiver mcastRcvr(asmGroupAddr, *this, version);
}

// Tests construction of source-specific multicast receiver
TEST_F(McastReceiverTest, SourceConstruction) {
    hycast::McastReceiver mcastRcvr(ssmGroupAddr, srcAddr, *this, version);
}

// Tests source-specific reception
TEST_F(McastReceiverTest, SourceReception) {
    hycast::McastReceiver mcastRcvr(ssmGroupAddr, srcAddr, *this, version);
    std::thread           rcvrThread =
            std::thread([this,mcastRcvr]() mutable {runReceiver(mcastRcvr);});
    sendProduct(ssmGroupAddr);
    ::usleep(100000);
    ::pthread_cancel(rcvrThread.native_handle());
    rcvrThread.join();
    EXPECT_EQ(prod.getInfo().getNumChunks(), chunkIndex);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
