/**
 * This file tests class `P2pMgr`.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: P2pMgr_test.cpp
 * Created On: Jul 1, 2019
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "error.h"
#include "P2pMgr.h"
#include "PeerFactory.h"
#include "SockAddr.h"

#include <condition_variable>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>
#include <unistd.h>

namespace {

/// The fixture for testing class `P2pMgr`
class P2pMgrTest : public ::testing::Test, public hycast::P2pMgrObs
{
protected:
    hycast::SockAddr        srvrAddr;
    hycast::SockAddr        srvrAddrs[3];
    hycast::SockAddr        snkAddr;
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
    hycast::ProdId       prodId;
    hycast::ProdSize        prodSize;
    hycast::SegSize         segSize;
    hycast::ProdInfo        prodInfo;
    hycast::SegId           segId;
    hycast::SegInfo         segInfo;
    char                    memData[1000];
    hycast::MemSeg          memSeg;
    hycast::PortPool        portPool;
    hycast::ServerPool      srvrSrvrPool;
    std::atomic<unsigned>   numAdded;

    // You can remove any or all of the following functions if its body
    // is empty.

    P2pMgrTest()
        : srvrAddr{"localhost:3880"}
        , srvrAddrs{
            // NB: Not Linux dynamic ports to obviate being acquired by sink
            hycast::SockAddr{"localhost:3880"},
            hycast::SockAddr{"localhost:3881"},
            hycast::SockAddr{"localhost:3882"}}
        , snkAddr{"localhost:3883"} // NB: Not a Linux dynamic port number
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
        , portPool(38840, 7) // NB: Linux Dynamic port numbers
        , srvrSrvrPool()
        , numAdded{0}
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

public:
    void added(hycast::Peer& peer)
    {
        if (++numAdded >= 2) // Once for client and once for server
            orState(CONNECTED);
    }

    void removed(hycast::Peer& peer)
    {}

    // Receiver-side
    bool shouldRequest(const hycast::ChunkId chunkId)
    {
        if (chunkId.isProdId()) {
            EXPECT_EQ(prodId, chunkId.getProdId());
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
        if (chunkId.isProdId()) {
            EXPECT_EQ(prodId, chunkId.getProdId());
            orState(PROD_REQUEST_RCVD);
            return prodInfo;
        }

        EXPECT_EQ(segId, chunkId.getSegId());
        orState(SEG_REQUEST_RCVD);
        return memSeg;
    }

    // Receiver-side
    bool hereIs(const hycast::ProdInfo& actual)
    {
        EXPECT_EQ(prodInfo, actual);
        orState(PROD_INFO_RCVD);

        return true;
    }

    // Receiver-side
    bool hereIs(hycast::TcpSeg& actual)
    {
        const hycast::SegSize size = actual.getInfo().getSegSize();
        EXPECT_EQ(segSize, size);

        char buf[size];
        actual.read(buf);

        EXPECT_EQ(0, ::memcmp(memSeg.getData(), buf, segSize));

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
};

#if 0
// Tests simple construction
TEST_F(P2pMgrTest, SimpleConstruction)
{
    hycast::P2pMgr p2pMgr(srvrAddr, 0, portPool, 0, srvrSrvrPool, *this);
}
#endif

// Tests exchanging data between two peers
TEST_F(P2pMgrTest, DataExchange)
{
    // Start server
    hycast::P2pMgr     srvrP2pMgr(srvrAddr, 0, portPool, 1, srvrSrvrPool, *this);
    std::thread        srvrThread(&P2pMgrTest::runP2pMgr, this,
            std::ref(srvrP2pMgr));

    // Start client
    hycast::ServerPool clntSrvrPool(std::set<hycast::SockAddr>{srvrAddr});
    hycast::P2pMgr     clntP2pMgr(snkAddr, 0, portPool, 1, clntSrvrPool, *this);
    std::thread        clntThread(&P2pMgrTest::runP2pMgr, this,
            std::ref(clntP2pMgr));

    waitForState(CONNECTED);

    // Start an exchange
    clntP2pMgr.notify(prodId);
    clntP2pMgr.notify(segId);

    // Wait for the exchange to complete
    waitForState(EXCHANGE_COMPLETE);

    clntP2pMgr.halt();
    clntThread.join();

    srvrP2pMgr.halt();
    srvrThread.join();
}

#if 0
// Tests multiple peers
TEST_F(P2pMgrTest, MultiplePeers)
{
    try {
        // Start servers
        hycast::P2pMgr srvrP2pMgrs[3] = {};
        std::thread    srvrThreads[3] = {};

        try {
            for (int i = 0; i < 3; ++i) {
                LOG_NOTE("Creating server %d", i);
                srvrP2pMgrs[i] = hycast::P2pMgr(srvrAddrs[i], 0, portPool, 1,
                        srvrSrvrPool, *this);
                LOG_NOTE("Executing server %d", i);
                srvrThreads[i] = std::thread(&P2pMgrTest::runP2pMgr,
                        std::ref(srvrP2pMgrs[i]));
            }
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex, "Server failure");
            throw;
        }

        // Start client
        LOG_NOTE("Starting client");
        hycast::ServerPool clntSrvrPool(std::set<hycast::SockAddr>{
            srvrAddrs[0], srvrAddrs[1], srvrAddrs[2]});
        hycast::P2pMgr     clntP2pMgr(snkAddr, 0, portPool, 2, clntSrvrPool,
                *this);
        std::thread        clntThread(&P2pMgrTest::runP2pMgr,
                std::ref(clntP2pMgr));

        // Wait for the client to connect to the servers
        LOG_NOTE("Waiting for client to connect to servers");
        while (clntP2pMgr.size() < 2)
            ::usleep(100000);

        // Start an exchange
        LOG_NOTE("Exchanging data");
        for (int i = 0; i < 3; ++i)
            srvrP2pMgrs[i].notify(chunkId);

        // Wait for the exchange to complete
        LOG_NOTE("Waiting for data-exchange to complete");
        waitForState(DONE);

        LOG_NOTE("Stopping client");
        clntP2pMgr.halt();
        clntThread.join();

        for (int i = 0; i < 3; ++i) {
            LOG_NOTE("Stopping server %d", i);
            srvrP2pMgrs[i].halt();
            srvrThreads[i].join();
        }
    }
    catch (const std::exception& ex) {
        LOG_ERROR(ex, "Couldn't test multiple peers");
    }
    catch (...) {
        LOG_ERROR("Caught ... exception");
        throw;
    }
}
#endif

} // namespace

static void myTerminate()
{
    LOG_FATAL("terminate() called %s an active exception",
            std::current_exception() ? "with" : "without");
    abort();
}

int main(int argc, char **argv)
{
  hycast::log_setName(::basename(argv[0]));
  //hycast::log_setLevel(hycast::LOG_LEVEL_TRACE);

  std::set_terminate(&myTerminate);
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
