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
class P2pMgrTest : public ::testing::Test, public hycast::PeerMsgRcvr
{
protected:
    hycast::SockAddr        srcAddr;
    hycast::SockAddr        srcAddrs[3];
    hycast::SockAddr        snkAddr;
    std::mutex              mutex;
    std::condition_variable cond;
    typedef enum {
        INIT,
        DONE
    } State;
    State                   state;
    hycast::ChunkId         chunkId;
    char                    memData[1000];
    hycast::MemChunk        memChunk;
    hycast::PortPool        portPool;
    hycast::ServerPool      srvrSrvrPool;

    // You can remove any or all of the following functions if its body
    // is empty.

    P2pMgrTest()
        : srcAddr{"localhost:3880"}
        , srcAddrs{
            // NB: Not Linux dynamic ports to obviate being acquired by receiver
            hycast::SockAddr{"localhost:3880"},
            hycast::SockAddr{"localhost:3881"},
            hycast::SockAddr{"localhost:3882"}}
        , snkAddr{"localhost:3883"} // NB: Not a dynamic port number
        , mutex{}
        , cond{}
        , state{INIT}
        , chunkId{1}
        , memData{0}
        , memChunk(chunkId, sizeof(memData), memData)
        , portPool(38820, 7) // NB: Dynamic port numbers
        , srvrSrvrPool()
    {}

    void setState(const State state) {
        std::lock_guard<decltype(mutex)> lock{mutex};

        this->state = state;
        cond.notify_one();
    }

    void waitForState(const State state) {
        std::unique_lock<decltype(mutex)> lock{mutex};

        while (this->state != state)
            cond.wait(lock);
    }

    // Objects declared here can be used by all tests in the test case for Error.
public:
    bool shouldRequest(
            const hycast::ChunkId&  notice,
            const hycast::SockAddr& rmtAddr) {
        EXPECT_EQ(chunkId, notice);
        return true;
    }

    hycast::MemChunk get(
            const hycast::ChunkId&  request,
            const hycast::SockAddr& rmtAddr) {
        EXPECT_EQ(chunkId, request);
        return memChunk;
    }

    void hereIs(
            hycast::WireChunk       wireChunk,
            const hycast::SockAddr& rmtAddr) {
        const hycast::ChunkSize n = wireChunk.getSize();
        char                    wireData[n];

        wireChunk.read(wireData);
        for (int i = 0; i < n; ++i)
            EXPECT_EQ(memData[i], wireData[i]);

        setState(DONE);
    }

    bool shouldRequest(
            const hycast::ChunkId& notice,
            hycast::Peer           peer) {
        EXPECT_EQ(chunkId, notice);
        return true;
    }

    hycast::MemChunk get(
            const hycast::ChunkId& request,
            hycast::Peer           peer) {
        EXPECT_EQ(chunkId, request);
        return memChunk;
    }

    void hereIs(
            hycast::WireChunk wireChunk,
            hycast::Peer      peer) {
        const hycast::ChunkSize n = wireChunk.getSize();
        char                    wireData[n];

        wireChunk.read(wireData);
        for (int i = 0; i < n; ++i)
            EXPECT_EQ(memData[i], wireData[i]);

        setState(DONE);
    }

    static void runP2pMgr(hycast::P2pMgr& p2pMgr) {
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
    hycast::P2pMgr p2pMgr(srcAddr, 0, portPool, 0, srvrSrvrPool, *this);
}
#endif

// Tests exchanging data
TEST_F(P2pMgrTest, DataExchange)
{
    // Start server
    hycast::P2pMgr     srvrP2pMgr(srcAddr, 0, portPool, 1, srvrSrvrPool, *this);
    std::thread        srvrThread(&P2pMgrTest::runP2pMgr, std::ref(srvrP2pMgr));

    // Start client
    hycast::ServerPool clntSrvrPool(std::set<hycast::SockAddr>{srcAddr});
    hycast::P2pMgr     clntP2pMgr(snkAddr, 0, portPool, 1, clntSrvrPool, *this);
    std::thread        clntThread(&P2pMgrTest::runP2pMgr, std::ref(clntP2pMgr));

    // Wait for the client to connect to the server
    while (clntP2pMgr.size() == 0)
        ::usleep(100000);

    // Start an exchange
    srvrP2pMgr.notify(chunkId);

    // Wait for the exchange to complete
    waitForState(DONE);

    clntP2pMgr.halt();
    clntThread.join();

    srvrP2pMgr.halt();
    srvrThread.join();
}

#if 1
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
                srvrP2pMgrs[i] = hycast::P2pMgr(srcAddrs[i], 0, portPool, 1,
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
            srcAddrs[0], srcAddrs[1], srcAddrs[2]});
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
