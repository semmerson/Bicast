/**
 * This file tests the multicast and P2P components.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: hycast_test.cpp
 * Created On: Dec 16, 2019
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "hycast.h"

#include "error.h"
#include "McastProto.h"
#include "P2pMgr.h"
#include "PortPool.h"
#include "ServerPool.h"

#include <condition_variable>
#include <gtest/gtest.h>
#include <mutex>

namespace {

/// The fixture for testing
class HycastTest :
        public ::testing::Test,
        public hycast::P2pMgrObs,
        public hycast::McastRcvrObs
{
protected:
    hycast::SockAddr        grpAddr; // Multicast group address
    hycast::InetAddr        sndrInetAddr;
    hycast::SockAddr        sndrSockAddr;
    hycast::SockAddr        rcvrSockAddr;
    std::mutex              mutex;
    std::condition_variable cond;
    typedef enum {
        INIT = 0,
        CONNECTED = 1,
        PROD_NOTICE_RCVD = 2,
        SEG_NOTICE_RCVD = 4,
        PROD_REQUEST_RCVD = 8,
        SEG_REQUEST_RCVD = 16,
        PROD_INFO_RCVD = 32,
        SEG_RCVD = 64,
        CLNT_PEER_STOPPED = 128,
        SRVR_PEER_STOPPED = 256,
        EXCHANGE_COMPLETE = CONNECTED |
               PROD_NOTICE_RCVD |
               SEG_NOTICE_RCVD |
               PROD_REQUEST_RCVD |
               SEG_REQUEST_RCVD |
               PROD_INFO_RCVD |
               SEG_RCVD,
        DONE = EXCHANGE_COMPLETE |
               CLNT_PEER_STOPPED |
               SRVR_PEER_STOPPED
    } State;
    State                   state;
    hycast::ProdIndex          prodId;
    hycast::ProdSize        prodSize;
    hycast::SegSize         segSize;
    hycast::ProdInfo        prodInfo;
    hycast::SegId           segId;
    hycast::SegInfo         segInfo;
    char                    memData[1000];
    hycast::MemSeg          memSeg;
    hycast::PortPool        portPool;
    hycast::ServerPool      sndrSrvrPool;
    std::atomic<int>        numToAdd;

    HycastTest()
        : grpAddr("232.1.1.1:3880")
        , sndrInetAddr{"localhost"}
        , sndrSockAddr{sndrInetAddr, 3880}
        , rcvrSockAddr{"localhost:3881"} // NB: Not a Linux dynamic port number
        , mutex{}
        , cond{}
        , prodId{1}
        , state{INIT}
        , prodSize{1000000}
        , segSize{sizeof(memData)}
        , prodInfo{prodId, prodSize, "product"}
        , segId(prodId, segSize)
        , segInfo(segId, prodSize, segSize)
        , memData{}
        , memSeg{segInfo, memData}
        , portPool(3882, 5) // NB: Linux Dynamic port numbers
        , sndrSrvrPool()
        , numToAdd{0}
    {
        ::memset(memData, 0xbd, segSize);
    }

    void setState(const State state) {
        std::lock_guard<decltype(mutex)> lock{mutex};

        this->state = state;
        cond.notify_one();
    }

    void orState(const State state)
    {
        std::lock_guard<decltype(mutex)> guard{mutex};
        this->state = static_cast<State>(this->state | state);
        cond.notify_all();
    }

    void waitForState(const State state) {
        std::unique_lock<decltype(mutex)> lock{mutex};

        while (this->state != state)
            cond.wait(lock);
    }

    // A peer was added to a set of peers
    void added(hycast::Peer& peer)
    {
        if (--numToAdd <= 0)
            orState(CONNECTED);
    }

    void removed(hycast::Peer& peer)
    {
        ++numToAdd;
    }

    // Receiver-side
    bool shouldRequest(const hycast::ChunkId chunkId)
    {
        if (chunkId.isProdIndex()) {
            EXPECT_EQ(prodId, chunkId.getProdIndex());
            orState(PROD_NOTICE_RCVD);
        }
        else {
            EXPECT_EQ(segId, chunkId.getSegId());
            orState(SEG_NOTICE_RCVD);
        }

        return true;
    }

    // Sender-side
    const hycast::OutChunk& get(const hycast::ChunkId chunkId)
    {
        if (chunkId.isProdIndex()) {
            EXPECT_EQ(prodId, chunkId.getProdIndex());
            orState(PROD_REQUEST_RCVD);
            return prodInfo;
        }

        EXPECT_EQ(segId, chunkId.getSegId());
        orState(SEG_REQUEST_RCVD);
        return memSeg;
    }

    // Receiver-side
    bool hereIsMcast(const hycast::ProdInfo& actual)
    {
        EXPECT_EQ(prodInfo, actual);
        orState(PROD_INFO_RCVD);

        return true;
    }

    // Receiver-side
    bool hereIs(hycast::TcpSeg& actual)
    {
        const hycast::SegSize size = actual.getSegInfo().getSegSize();
        EXPECT_EQ(segSize, size);

        char buf[size];
        actual.read(buf);

        EXPECT_EQ(0, ::memcmp(memSeg.data(), buf, segSize));

        orState(SEG_RCVD);

        return true;
    }

    void runP2pMgr(hycast::P2pMgr& p2pMgr) {
        try {
            LOG_DEBUG("Executing p2pMgr");
            p2pMgr();
        }
        catch (const std::exception& ex) {
            LOG_DEBUG(ex, "Caught std::exception");
        }
        catch (...) {
            LOG_DEBUG("Caught ... exception");
            throw;
        }
    }

    void runRcvr(hycast::McastRcvr& rcvr)
    {
        // Notify the multicast sender that the receiver is ready
        {
            /*
            std::lock_guard<decltype(mutex)> guard{mutex};
            ready = true;
            cond.notify_one();
            */
        }

        //  Receive the multicast
        rcvr(); // Returns on `rcvr.halt()`
    }
};

// Tests a sender and one receiver
TEST_F(HycastTest, SingleReceiver)
{
    numToAdd = 2;

    // Start sender
        // Start P2P component
        hycast::P2pMgr sndrP2pMgr(sndrSockAddr, 0, portPool, 1, sndrSrvrPool,
                *this);
        std::thread    sndrThread(&HycastTest::runP2pMgr, this,
                std::ref(sndrP2pMgr));

        // Create multicast component
        hycast::UdpSock   sndrMcastSock{grpAddr};
        hycast::McastSndr mcastSndr{sndrMcastSock};

    // Start receiver
        // Start P2P component
        hycast::ServerPool rcvrSrvrPool(std::set<hycast::SockAddr>{sndrSockAddr});
        hycast::P2pMgr     rcvrP2pMgr(rcvrSockAddr, 0, portPool, 1, rcvrSrvrPool,
                *this);
        std::thread        rcvrThread(&HycastTest::runP2pMgr, this,
                std::ref(rcvrP2pMgr));

        // Start multicast component
        hycast::UdpSock   rcvrMcastSock{grpAddr, sndrInetAddr};
        hycast::McastRcvr mcastRcvr{rcvrMcastSock, *this};
        std::thread       rcvrThread(&HycastTest::runRcvr, this,
                std::ref(mcastRcvr));

    waitForState(CONNECTED);

    // Start an exchange
    sndrP2pMgr.notify(prodId);
    sndrP2pMgr.notify(segId);

    // Wait for the exchange to complete
    waitForState(EXCHANGE_COMPLETE);

    rcvrP2pMgr.halt();
    rcvrThread.join();

    sndrP2pMgr.halt();
    sndrThread.join();
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
