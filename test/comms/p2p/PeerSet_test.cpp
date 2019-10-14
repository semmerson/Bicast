/**
 * This file tests class `PeerSet`.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerSet_test.cpp
 * @author: Steven R. Emmerson
 */
#include "config.h"

#include "error.h"
#include "Interface.h"
#include "PeerSet.h"
#include "Thread.h"

#include <gtest/gtest.h>

namespace {

// The fixture for testing class PeerSet.
class PeerSetTest : public ::testing::Test, public hycast::PeerSetServer
{
protected:
    /**
     * Data-source.
     */
    class Source {
        hycast::PeerSetServer& peerSetServer;
        hycast::SrvrSctpSock   srvrSock;
        hycast::Thread         thread;
        hycast::PeerSet        peerSet;
        hycast::Cue            cue;

        void runSource() {
            try {
                for (;;) {
                    auto sock = srvrSock.accept();
                    try {
                        hycast::PeerMsgSndr peer{sock};
                        EXPECT_TRUE(peerSet.tryInsert(peer));
                        cue.cue();
                    }
                    catch (const std::exception& ex) {
                        LOG_WARN(ex, "Couldn't accept remote peer " +
                                std::to_string(sock));
                    }
                }
            }
            catch (const std::exception& ex) {
                LOG_ERROR(ex, "Data-source threw an exception");
            }
        }

    public:
        Source( const hycast::InetSockAddr& sourceSockAddr,
                hycast::PeerSetServer&      peerSetServer)
            : peerSetServer(peerSetServer)
            , srvrSock{sourceSockAddr, hycast::PeerMsgSndr::getNumStreams()}
            , thread{}
            , peerSet{peerSetServer, 2}
            , cue{}
        {
            srvrSock.listen();
            thread = hycast::Thread([this]{runSource();});
        }

        void send(const hycast::ProdInfo& prodInfo)
        {
            cue.wait();
            peerSet.notify(prodInfo.getIndex());
            auto numChunks = prodInfo.getNumChunks();
            for (hycast::ChunkIndex chunkIndex = 0;
                    chunkIndex < numChunks; ++chunkIndex) {
                try {
                    hycast::ChunkId chunkId{prodInfo, chunkIndex};
                    peerSet.notify(chunkId);
                }
                catch (const std::exception& ex) {
                    std::throw_with_nested(hycast::RUNTIME_ERROR(
                            "Couldn't notify about chunk " +
                            chunkIndex.to_string()));
                }
            }
        }
    };

    hycast::ProdStore        prodStore;
    hycast::ProdIndex        prodIndex;
    hycast::ProdSize         prodSize;
    hycast::ProdInfo         prodInfo;
    hycast::ChunkIndex::type numChunks;
    char                     chunkData[hycast::ChunkSize::maxSize];
    hycast::PortNumber       srvrPortNum;
    hycast::InetSockAddr     srvr1Addr;
    hycast::InetSockAddr     srvr2Addr;
    hycast::ChunkIndex::type numRcvdChunks;
    hycast::Barrier          barrier;
    Source                   source1;
    Source                   source2;

    PeerSetTest()
        : prodStore{}
        , prodIndex{1}
        , prodSize{1000000}
        , prodInfo{prodIndex, "product", prodSize}
        , numChunks{prodInfo.getNumChunks()}
        , srvrPortNum{38800}
        , srvr1Addr{"127.0.0.1", srvrPortNum}
        , srvr2Addr{hycast::Interface{ETHNET_IFACE_NAME}.getInetAddr(AF_INET),
                srvrPortNum}
        , numRcvdChunks{0}
        , barrier{2}
        , source1{srvr1Addr, *this}
        , source2{srvr2Addr, *this}
    {
        ::memset(chunkData, 0xbd, sizeof(chunkData));
    }

    hycast::PeerMsgSndr getClientPeer(const hycast::InetSockAddr& serverSockAddr) {
        hycast::SctpSock sock{hycast::PeerMsgSndr::getNumStreams()};
        sock.connect(serverSockAddr);
        return hycast::PeerMsgSndr{sock};
    }

    // Begin implementation of `PeerSetServer` interface

    hycast::ChunkId getEarliestMissingChunkId() {
        return hycast::ChunkId{};
    }

    hycast::Backlogger getBacklogger(
            const hycast::ChunkId& earliest,
            hycast::PeerMsgSndr&          peer)
    {
        return hycast::Backlogger(peer, earliest, prodStore);
    }
    void peerStopped(const hycast::InetSockAddr& peerAddr)
    {}
    bool shouldRequest(const hycast::ProdIndex& index)
    {
        EXPECT_EQ(prodIndex, index);
        return true;
    }
    bool shouldRequest(const hycast::ChunkId& chunkId)
    {
        EXPECT_EQ(prodIndex, chunkId.getProdIndex());
        EXPECT_GT(numChunks, chunkId.getChunkIndex());
        return true;
    }
    bool get(const hycast::ProdIndex& index, hycast::ProdInfo& info)
    {
        EXPECT_EQ(prodIndex, index);
        info = prodInfo;
        return true;
    }
    bool get(const hycast::ChunkId& chunkId, hycast::ActualChunk& chunk)
    {
        EXPECT_EQ(prodIndex, chunkId.getProdIndex());
        auto chunkIndex = chunkId.getChunkIndex();
        EXPECT_GT(numChunks, chunkIndex);
        const hycast::ChunkInfo chunkInfo{prodInfo, chunkIndex};
        chunk = hycast::ActualChunk(chunkInfo, chunkData);
        return true;
    }
    hycast::RecvStatus receive(
            const hycast::ProdInfo&     info,
            const hycast::InetSockAddr& peerAddr)
    {
        EXPECT_EQ(prodInfo, info);
        EXPECT_TRUE((srvr1Addr == peerAddr) || (srvr2Addr == peerAddr));
        return hycast::RecvStatus{};
    }
    hycast::RecvStatus receive(
            hycast::SocketChunk&        chunk,
            const hycast::InetSockAddr& peerAddr)
    {
        EXPECT_TRUE((srvr1Addr == peerAddr) || (srvr2Addr == peerAddr));
        auto chunkIndex = chunk.getIndex();
        EXPECT_GT(numChunks, chunkIndex);
        EXPECT_EQ(prodIndex, chunk.getProdIndex());
        EXPECT_EQ(prodSize, chunk.getProdSize());
        EXPECT_EQ(hycast::ChunkSize::defaultSize, chunk.getCanonSize());
        auto chunkSize = chunk.getSize();
        EXPECT_EQ(prodInfo.getChunkSize(chunkIndex), chunkSize);
        char data[static_cast<size_t>(chunkSize)];
        chunk.drainData(data, chunkSize);
        EXPECT_EQ(0, ::memcmp(chunkData, data, chunkSize)); // Most time here
        if (++numRcvdChunks == numChunks)
            barrier.wait();
        return hycast::RecvStatus{};
    }
};

// Tests default construction
TEST_F(PeerSetTest, DefaultConstruction) {
    hycast::PeerSet peerSet{*this, 1}; // 1 second maximum residence
}

// Tests construction with invalid argument
TEST_F(PeerSetTest, InvalidConstruction) {
    EXPECT_THROW(hycast::PeerSet(*this, 0, 2), std::invalid_argument);
}

// Tests inserting a peer and incrementing its value
TEST_F(PeerSetTest, IncrementPeerValue) {
    try {
        //std::set_terminate([]{std::cerr << "In terminate()\n"; ::pause();});
        hycast::PeerSet  peerSet{*this, 2};
        auto             peer = getClientPeer(srvr1Addr);
        EXPECT_TRUE(peerSet.tryInsert(peer));
        EXPECT_EQ(1, peerSet.size());
        EXPECT_NO_THROW(peerSet.incValue(peer));
    }
    catch (const std::exception& e) {
        hycast::log_error(e);
    }
}

// Tests inserting the same peer twice
TEST_F(PeerSetTest, DuplicatePeerInsertion) {
    hycast::PeerMsgSndr     peer{getClientPeer(srvr1Addr)};
    hycast::PeerSet  peerSet{*this, 2};
    EXPECT_TRUE(peerSet.tryInsert(peer));
    EXPECT_FALSE(peerSet.tryInsert(peer));
}

// Tests removing the worst peer from a 1-peer set
TEST_F(PeerSetTest, RemoveWorst) {
    try {
        hycast::PeerSet  peerSet{*this, 0, 1};

        hycast::PeerMsgSndr peer1{getClientPeer(srvr1Addr)};
        EXPECT_TRUE(peerSet.tryInsert(peer1));

        hycast::PeerMsgSndr peer2{getClientPeer(srvr2Addr)};
        EXPECT_TRUE(peerSet.tryInsert(peer2));

        EXPECT_EQ(1, peerSet.size());
    }
    catch (const std::exception& e) {
        hycast::log_error(e);
    }
}

// Tests transmitting a product
TEST_F(PeerSetTest, Transmit) {
    hycast::PeerMsgSndr     peer = getClientPeer(srvr1Addr);
    hycast::PeerSet  peerSet{*this, 2};
    EXPECT_TRUE(peerSet.tryInsert(peer));
    source1.send(prodInfo);
    barrier.wait();
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
