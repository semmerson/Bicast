/**
 * This file tests class `PeerSet`.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: PeetSet.cpp
 * Created On: June 11, 2019
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "PeerFactory.h"
#include "PeerSet.h"
#include "SockAddr.h"

#include <atomic>
#include <condition_variable>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>

namespace {

/// The fixture for testing class `PeerSet`
class PeerSetTest : public ::testing::Test, public hycast::PeerObs,
        public hycast::PeerSet::Observer
{
protected:
    friend class Receiver;

    typedef enum {
        INIT = 0,
        LISTENING = 1,
        PROD_NOTICE_RCVD = 2,
        SEG_NOTICE_RCVD = 4,
        PROD_REQUEST_RCVD = 8,
        SEG_REQUEST_RCVD = 16,
        PROD_INFO_RCVD = 32,
        SEG_RCVD = 64,
        CLNT_PEER_STOPPED = 128,
        SRVR_PEER_STOPPED = 256,
        EXCHANGE_COMPLETE = LISTENING |
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
    hycast::SockAddr        srvrAddr;
    hycast::PortPool        portPool;
    std::mutex              mutex;
    std::condition_variable cond;
    hycast::ProdIndex       prodIndex;
    hycast::ProdSize        prodSize;
    hycast::SegSize         segSize;
    hycast::ProdInfo        prodInfo;
    hycast::SegId           segId;
    hycast::SegInfo         segInfo;
    char                    memData[1000];
    hycast::MemSeg          memSeg;
    hycast::PeerFactory     factory;
    hycast::Peer            srvrPeer;
    hycast::Peer            clntPeer;

    // You can remove any or all of the following functions if its body
    // is empty.

    PeerSetTest()
        : state{INIT}
        , srvrAddr{"localhost:3880"}
        /*
         * 3 potential port numbers for the server's 2 temporary servers because
         * the initial client connection could use one
         */
        , portPool(3881, 3)
        , mutex{}
        , cond{}
        , prodIndex{1}
        , prodSize{1000000}
        , segSize{sizeof(memData)}
        , prodInfo{prodIndex, prodSize, "product"}
        , segId(prodIndex, segSize)
        , segInfo(segId, prodSize, segSize)
        , memData{}
        , memSeg{segInfo, memData}
        , factory()
        , srvrPeer()
        , clntPeer()
    {
        ::memset(memData, 0xbd, segSize);
        factory = hycast::PeerFactory(srvrAddr, 1, portPool, *this);
    }

public:
    void orState(const State state)
    {
        std::lock_guard<decltype(mutex)> guard{mutex};
        this->state = static_cast<State>(this->state | state);
        cond.notify_all();
    }

    void waitForState(const State state)
    {
        std::unique_lock<decltype(mutex)> lock{mutex};
        while (this->state != state)
            cond.wait(lock);
    }

    // Receiver-side
    bool shouldRequest(
            const hycast::ProdIndex actual,
            hycast::Peer&           peer)
    {
        EXPECT_EQ(prodIndex, actual);
        orState(PROD_NOTICE_RCVD);

        return true;
    }

    // Receiver-side
    bool shouldRequest(
            const hycast::SegId&    actual,
            hycast::Peer&           peer)
    {
        EXPECT_EQ(segId, actual);
        orState(SEG_NOTICE_RCVD);

        return true;
    }

    // Sender-side
    hycast::ProdInfo get(
            const hycast::ProdIndex actual,
            hycast::Peer&           peer)
    {
        EXPECT_EQ(prodIndex, actual);
        orState(PROD_REQUEST_RCVD);
        return prodInfo;
    }

    // Sender-side
    hycast::MemSeg get(
            const hycast::SegId&    actual,
            hycast::Peer&           peer)
    {
        EXPECT_EQ(segId, actual);
        orState(SEG_REQUEST_RCVD);
        return memSeg;
    }

    // Receiver-side
    bool hereIs(
            const hycast::ProdInfo& actual,
            hycast::Peer&           peer)
    {
        EXPECT_EQ(prodInfo, actual);
        orState(PROD_INFO_RCVD);

        return true;
    }

    // Receiver-side
    bool hereIs(
            hycast::TcpSeg&         actual,
            hycast::Peer&           peer)
    {
        const hycast::SegSize size = actual.getSegInfo().getSegSize();
        EXPECT_EQ(segSize, size);

        char buf[size];
        actual.getData(buf);

        EXPECT_EQ(0, ::memcmp(memSeg.data(), buf, segSize));

        orState(SEG_RCVD);

        return true;
    }

    void stopped(hycast::Peer peer)
    {
        EXPECT_TRUE(peer == srvrPeer || peer == clntPeer);

        if (peer == srvrPeer) {
            orState(SRVR_PEER_STOPPED);
        }
        else if (peer == clntPeer) {
            orState(CLNT_PEER_STOPPED);
        }
    }

    void runServer(hycast::PeerSet& peerSet)
    {
        orState(LISTENING);
        srvrPeer = factory.accept();
        EXPECT_EQ(0, peerSet.size());
        peerSet.activate(srvrPeer); // Executes peer on new thread
        EXPECT_EQ(1, peerSet.size());
    }
};

// Tests default construction
TEST_F(PeerSetTest, DefaultConstruction)
{
    hycast::PeerSet{};
}

// Tests complete exchange (notice, request, delivery)
TEST_F(PeerSetTest, Exchange)
{
    // Start server
    hycast::PeerSet srvrPeerSet{*this};
    std::thread     srvrThread(&PeerSetTest::runServer, this,
            std::ref(srvrPeerSet));

    waitForState(LISTENING);

    // Start client
    hycast::PeerSet clntPeerSet{*this};
    clntPeer = factory.connect(srvrAddr);
    EXPECT_EQ(0, clntPeerSet.size());
    clntPeerSet.activate(clntPeer); // Executes `clntPeer` on new thread
    EXPECT_EQ(1, clntPeerSet.size());

    // Start an exchange
    clntPeerSet.notify(prodIndex);
    clntPeerSet.notify(segId);

    // Wait for the exchange to complete
    waitForState(EXCHANGE_COMPLETE);

    // Terminate clnt peer. Causes server-peer to terminate.
    clntPeer.halt();

    // Wait for the peers to be removed from their peer-sets
    waitForState(DONE);

    srvrThread.join();
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
