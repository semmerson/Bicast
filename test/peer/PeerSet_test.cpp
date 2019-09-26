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

/// The fixture for testing class `Peer`
class PeerSetTest : public ::testing::Test, public hycast::PeerMsgRcvr,
        public hycast::PeerSet::Observer
{
protected:
    friend class Receiver;

    hycast::SockAddr        srcAddr;
    std::mutex              mutex;
    std::condition_variable cond;
    enum {
        INIT,
        READY,
        DONE
    }                   state;
    hycast::ChunkId     chunkId;
    char                memData[1000];
    hycast::MemChunk    memChunk;
    hycast::PeerFactory factory;
    hycast::Peer        srvrPeer;
    hycast::Peer        clntPeer;
    hycast::PeerSet     srvrPeerSet;
    hycast::PeerSet     clntPeerSet;
    bool                srvrPeerStopped;
    bool                clntPeerStopped;

    // You can remove any or all of the following functions if its body
    // is empty.

    PeerSetTest()
        : srcAddr{"localhost:38800"}
        , mutex{}
        , cond{}
        , state{INIT}
        , chunkId{1}
        , memData{0}
        , memChunk(chunkId, sizeof(memData), memData)
        , factory()
        , srvrPeer()
        , clntPeer()
        , srvrPeerSet(*this)
        , clntPeerSet(*this)
        , srvrPeerStopped{false}
        , clntPeerStopped{false}
    {
        // You can do set-up work for each test here.
    }

    virtual ~PeerSetTest()
    {
        // You can do clean-up work that doesn't throw exceptions here.
    }

    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:

    virtual void SetUp()
    {
        // Code here will be called immediately after the constructor (right
        // before each test).
    }

    virtual void TearDown()
    {
        // Code here will be called immediately after each test (right
        // before the destructor).
    }

    // Objects declared here can be used by all tests in the test case for Error.

public:
    bool shouldRequest(
            const hycast::ChunkId&  notice,
            const hycast::SockAddr& rmtAddr)
    {
        EXPECT_TRUE(chunkId == notice);
        return true;
    }

    hycast::MemChunk get(
            const hycast::ChunkId&  request,
            const hycast::SockAddr& rmtAddr)
    {
        EXPECT_EQ(chunkId, request);
        return memChunk;
    }

    void hereIs(
            hycast::WireChunk       wireChunk,
            const hycast::SockAddr& rmtAddr)
    {
        const hycast::ChunkSize n = wireChunk.getSize();
        char                    wireData[n];

        wireChunk.read(wireData);
        for (int i = 0; i < n; ++i)
            EXPECT_EQ(memData[i], wireData[i]);

        {
            std::lock_guard<decltype(mutex)> lock{mutex};
            state = DONE;
            cond.notify_one();
        }
    }

    bool shouldRequest(
            const hycast::ChunkId& notice,
            hycast::Peer           peer)
    {
        EXPECT_TRUE(chunkId == notice);
        return true;
    }

    hycast::MemChunk get(
            const hycast::ChunkId& request,
            hycast::Peer           peer)
    {
        EXPECT_EQ(chunkId, request);
        return memChunk;
    }

    void hereIs(
            hycast::WireChunk wireChunk,
            hycast::Peer      peer)
    {
        const hycast::ChunkSize n = wireChunk.getSize();
        char                    wireData[n];

        wireChunk.read(wireData);
        for (int i = 0; i < n; ++i)
            EXPECT_EQ(memData[i], wireData[i]);

        {
            std::lock_guard<decltype(mutex)> lock{mutex};
            state = DONE;
            cond.notify_one();
        }
    }

    void stopped(hycast::Peer peer)
    {
        EXPECT_TRUE(peer == srvrPeer || peer == clntPeer);

        std::lock_guard<decltype(mutex)> guard{mutex};

        if (peer == srvrPeer) {
            srvrPeerStopped = true;
        }
        else if (peer == clntPeer) {
            clntPeerStopped = true;
        }

        cond.notify_one();
    }

    void runServer()
    {
        srvrPeer = factory.accept();
        EXPECT_EQ(0, srvrPeerSet.size());
        srvrPeerSet.activate(srvrPeer); // Executes peer on new thread
        EXPECT_EQ(1, srvrPeerSet.size());

        std::lock_guard<decltype(mutex)> lock{mutex};
        state = READY;
        cond.notify_one();
    }
};

// Tests default construction
TEST_F(PeerSetTest, DefaultConstruction)
{}

// Tests complete exchange (notice, request, delivery)
TEST_F(PeerSetTest, Exchange)
{
    // 3 ports for client's temporary servers in case initial connection uses one
    hycast::PortPool portPool(38801, 3);

    // Start server
    factory = hycast::PeerFactory(srcAddr, 1, portPool, *this);
    std::thread      srvrThread(&PeerSetTest::runServer, this);

    // Start client
    clntPeer = factory.connect(srcAddr);
    EXPECT_EQ(0, clntPeerSet.size());
    clntPeerSet.activate(clntPeer); // Executes `clntPeer` on new thread
    EXPECT_EQ(1, clntPeerSet.size());

    // Wait for the peers to connect
    {
        std::unique_lock<decltype(mutex)> lock{mutex};
        while (state != READY)
            cond.wait(lock);
    }

    // Set the direction of the exchange
    hycast::PeerSet& srcPeerSet = srvrPeerSet;
    hycast::Peer&    snkPeer = clntPeer;

    // Start an exchange
    srcPeerSet.notify(chunkId);

    // Wait for the exchange to complete
    {
        std::unique_lock<decltype(mutex)> lock{mutex};
        while (state != DONE)
            cond.wait(lock);
    }

    // Terminate sink peer. Causes source-peer to terminate.
    snkPeer.halt();

    // Wait for the peers to be removed from their peer-sets
    {
        std::unique_lock<decltype(mutex)> lock{mutex};
        while (!clntPeerStopped && !srvrPeerStopped)
            cond.wait(lock);
    }

    srvrThread.join();
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
