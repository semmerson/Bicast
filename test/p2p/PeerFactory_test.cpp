/**
 * This file tests class `PeerFactory`.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: PeetFactory_test.cpp
 * Created On: June 6, 2019
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "error.h"
#include "PeerFactory.h"
#include <condition_variable>
#include <gtest/gtest.h>
#include <main/inet/SockAddr.h>
#include <mutex>
#include <thread>

namespace {

/// The fixture for testing class `PeerFactory`
class PeerFactoryTest : public ::testing::Test, public hycast::PeerMgrApi
{
protected:
    friend class Receiver;

    typedef enum {
        INIT = 0,
        PUB_PEER_CREATED = 0x1,
        PROD_NOTICE_RCVD = 0x2,
        SEG_NOTICE_RCVD = 0x4,
        PROD_REQUEST_RCVD = 0x8,
        SEG_REQUEST_RCVD = 0x10,
        PROD_INFO_RCVD = 0x20,
        SEG_RCVD = 0x40,
        DONE = PUB_PEER_CREATED |
               PROD_NOTICE_RCVD |
               SEG_NOTICE_RCVD |
               PROD_REQUEST_RCVD |
               SEG_REQUEST_RCVD |
               PROD_INFO_RCVD |
               SEG_RCVD
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
    hycast::Peer            pubPeer;

    // You can remove any or all of the following functions if its body
    // is empty.

    PeerFactoryTest()
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
        , pubPeer()
    {
        ::memset(memData, 0xbd, segSize);
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

    void pathToPub(hycast::Peer peer)
    {}

    void noPathToPub(hycast::Peer peer)
    {}

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

    void runPub(hycast::PeerFactory& factory)
    {
        try {
            pubPeer = factory.accept(hycast::NodeType::PUBLISHER);
            if (pubPeer) {
                orState(PUB_PEER_CREATED);
                pubPeer();
            }
        }
        catch (const std::exception& ex) {
            hycast::log_error(ex);
        }
        catch (...) {
            LOG_NOTE("Server thread cancelled");
        }
    }
};

// Tests closing the factory
TEST_F(PeerFactoryTest, FactoryClosure)
{
    // Start a server. Calls `::listen()`.
    hycast::PeerFactory factory(srvrAddr, 1, portPool, *this);
    std::thread         pubThread(&PeerFactoryTest::runPub, this,
            std::ref(factory));

    try {
        // Close the factory. Causes `runPub()` to return.
        factory.close();

        pubThread.join();
    }
    catch (const std::exception& ex) {
        hycast::log_error(ex);
        pubThread.join();
        throw;
    }
}

// Tests complete exchange (notice, request, delivery)
TEST_F(PeerFactoryTest, Exchange)
{
    // Start a publisher. Calls `::listen()`.
    hycast::PeerFactory factory(srvrAddr, 1, portPool, *this);
    std::thread         pubThread(&PeerFactoryTest::runPub, this,
            std::ref(factory));

    // Start a subscriber
    hycast::Peer subPeer = factory.connect(srvrAddr,
            hycast::NodeType::NO_PATH_TO_PUBLISHER);
    std::thread  subThread(subPeer); // `clntPeer` is connected

    // Start an exchange
    waitForState(PUB_PEER_CREATED);
    pubPeer.notify(prodIndex);
    pubPeer.notify(segId);

    waitForState(DONE);

    // Causes `subPeer()` to return and `pubThread` to terminate
    subPeer.halt();

    subThread.join();
    pubThread.join();
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
