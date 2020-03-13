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
#include "SockAddr.h"

#include <condition_variable>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>

namespace {

/// The fixture for testing class `PeerFactory`
class PeerFactoryTest : public ::testing::Test, public hycast::PeerObs
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
        DONE = LISTENING |
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

    void pathToSrc(hycast::Peer peer)
    {}

    void noPathToSrc(hycast::Peer peer)
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

    void runServer(hycast::PeerFactory& factory)
    {
        try {
            orState(LISTENING);
            hycast::Peer srvrPeer = factory.accept();
            if (srvrPeer)
                srvrPeer();
        }
        catch (const std::exception& ex) {
            hycast::log_error(ex);
            throw;
        }
        catch (...) {
            LOG_NOTE("Server thread cancelled");
            throw;
        }
    }
};

// Tests closing the factory
TEST_F(PeerFactoryTest, FactoryClosure)
{
    // Start a server. Calls `::listen()`.
    hycast::NodeType    nodeType{hycast::NodeType::NO_PATH_TO_SOURCE};
    hycast::PeerFactory factory(srvrAddr, 1, portPool, *this, nodeType);
    std::thread         srvrThread(&PeerFactoryTest::runServer, this,
            std::ref(factory));

    try {
        // Ensure `listen()` called to test shutdown() effect on accept()
        waitForState(LISTENING);

        // Close the factory. Causes `runServer()` to return.
        factory.close();

        srvrThread.join();
    }
    catch (const std::exception& ex) {
        hycast::log_error(ex);
        srvrThread.join();
        throw;
    }
}

// Tests complete exchange (notice, request, delivery)
TEST_F(PeerFactoryTest, Exchange)
{
    // Start a server. Calls `::listen()`.
    hycast::NodeType    nodeType{hycast::NodeType::NO_PATH_TO_SOURCE};
    hycast::PeerFactory factory(srvrAddr, 1, portPool, *this, nodeType);
    std::thread         srvrThread(&PeerFactoryTest::runServer, this,
            std::ref(factory));

    // Start a client peer
    hycast::Peer clntPeer = factory.connect(srvrAddr);
    std::thread  clntThread(clntPeer); // `clntPeer` is connected

    // Start an exchange
    clntPeer.notify(prodIndex);
    clntPeer.notify(segId);

    waitForState(DONE);

    // Causes `clntPeer()` to return and `srvrThread` to terminate
    clntPeer.halt();

    clntThread.join();
    srvrThread.join();
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
