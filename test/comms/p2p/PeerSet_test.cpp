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
#include "HycastTypes.h"
#include "Interface.h"
#include "logging.h"
#include "PeerMsgRcvr.h"
#include "PeerSet.h"
#include "ProdInfo.h"
#include "ProdStore.h"
#include "SctpSock.h"

#include <gtest/gtest.h>
#include <pthread.h>
#include <thread>
#include <unistd.h>

namespace {

// The fixture for testing class PeerSet.
class PeerSetTest : public ::testing::Test
{
protected:
    /**
     * Thread-safe receiver of peer-messages that's a black hole.
     */
    class MsgRcvr final : public hycast::PeerMsgRcvr
    {
    public:
        void recvNotice(const hycast::ProdInfo& info, const hycast::Peer& peer)
        {}
        void recvNotice(const hycast::ChunkId& id, const hycast::Peer& peer)
        {}
        void recvRequest(const hycast::ProdIndex& index, const hycast::Peer& peer)
        {}
        void recvRequest(const hycast::ChunkId& info, const hycast::Peer& peer)
        {}
        void recvData(hycast::LatentChunk latentChunk, const hycast::Peer& peer)
        {
                latentChunk.discard();
        }
    };

    /**
     * Data-source that's a receiving black hole.
     */
    class Source {
        std::thread thread;
        void runSource(hycast::SrvrSctpSock sourceSock) {
            try {
                MsgRcvr  srvrMsgRcvr{};
                hycast::ProdStore prodStore{};
                hycast::PeerSet   peerSet{prodStore, srvrMsgRcvr, 2};
                for (;;) {
                    auto sock = sourceSock.accept();
                    try {
                        hycast::Peer peer{sock};
                        peerSet.tryInsert(peer);
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
        Source(const hycast::InetSockAddr& sourceSockAddr)
            : thread{}
        {
            hycast::SrvrSctpSock sourceSock{sourceSockAddr,
                hycast::Peer::getNumStreams()};
            sourceSock.listen();
            thread = std::thread([this,sourceSock]{runSource(sourceSock);});
        }
        ~Source() {
            if (thread.joinable()) {
                auto status = ::pthread_cancel(thread.native_handle());
                if (status)
                    hycast::log_error(hycast::SYSTEM_ERROR(
                            "Couldn't cancel data-source thread", status));
                thread.join();
            }
        }
    };

    PeerSetTest()
        : source1SockAddr{"127.0.0.1", 38800}
        , source2SockAddr{hycast::Interface{ETHNET_IFACE_NAME}.getInetAddr(AF_INET),
                38800}
        , prodStore{}
    {}

    hycast::Peer getClientPeer(const hycast::InetSockAddr& serverSockAddr) {
        hycast::SctpSock sock{hycast::Peer::getNumStreams()};
        sock.connect(serverSockAddr);
        return hycast::Peer{sock};
    }

    // Objects declared here can be used in all TEST_F tests
    const hycast::InetSockAddr source1SockAddr;
    const hycast::InetSockAddr source2SockAddr;
    const hycast::ProdInfo     prodInfo{1, "product", 100000};
    const hycast::ChunkInfo    chunkInfo{prodInfo, 2};
    MsgRcvr                    msgRcvr{};
    hycast::ProdStore          prodStore;
};

// Tests default construction
TEST_F(PeerSetTest, DefaultConstruction) {
    MsgRcvr         msgRcvr{};
    hycast::PeerSet peerSet{prodStore, msgRcvr, 1}; // 1 second maximum residence
}

// Tests construction with invalid argument
TEST_F(PeerSetTest, InvalidConstruction) {
    MsgRcvr msgRcvr{};
    EXPECT_THROW(hycast::PeerSet(prodStore, msgRcvr, 2, 0), std::invalid_argument);
}

// Tests inserting a peer and incrementing its value
TEST_F(PeerSetTest, IncrementPeerValue) {
    try {
        //std::set_terminate([]{std::cerr << "In terminate()\n"; ::pause();});
        Source           source{source1SockAddr};
        MsgRcvr          msgRcvr{};
        hycast::PeerSet  peerSet{prodStore, msgRcvr, 2};
        auto             peer = getClientPeer(source1SockAddr);
        EXPECT_TRUE(peerSet.tryInsert(peer));
        EXPECT_EQ(1, peerSet.size());
        EXPECT_NO_THROW(peerSet.incValue(peer));
    }
    catch (const std::exception& e) {
        hycast::log_error(e);
    }
}

// Tests removing the worst peer from a 1-peer set
TEST_F(PeerSetTest, RemoveWorst) {
    try {
        Source           source1{source1SockAddr};
        MsgRcvr          msgRcvr{};
        hycast::PeerSet  peerSet{prodStore, msgRcvr, 0, 1};

        hycast::Peer peer1{getClientPeer(source1SockAddr)};
        EXPECT_TRUE(peerSet.tryInsert(peer1));

        Source       source2{source2SockAddr};
        hycast::Peer peer2{getClientPeer(source2SockAddr)};
        EXPECT_TRUE(peerSet.tryInsert(peer2));

        EXPECT_EQ(1, peerSet.size());
    }
    catch (const std::exception& e) {
        hycast::log_error(e);
    }
}

// Tests inserting a peer and sending notices
TEST_F(PeerSetTest, PeerInsertionAndNotices) {
    Source           source{source1SockAddr};
    hycast::Peer     peer{getClientPeer(source1SockAddr)};
    MsgRcvr          msgRcvr{};
    hycast::PeerSet  peerSet{prodStore, msgRcvr, 2};
    EXPECT_TRUE(peerSet.tryInsert(peer));
    peerSet.sendNotice(prodInfo);
    peerSet.sendNotice(chunkInfo.getId());
    ::usleep(100000);
}

// Tests inserting the same peer twice
TEST_F(PeerSetTest, DuplicatePeerInsertion) {
    Source server{source1SockAddr};
    hycast::Peer     peer{getClientPeer(source1SockAddr)};
    MsgRcvr          msgRcvr{};
    hycast::PeerSet  peerSet{prodStore, msgRcvr, 2};
    EXPECT_TRUE(peerSet.tryInsert(peer));
    EXPECT_FALSE(peerSet.tryInsert(peer));
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
