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
     * Thread-safe peer-manager that discards everything.
     */
    class ClientMsgRcvr final : public hycast::PeerMsgRcvr {
        hycast::ProdInfo  prodInfo;
        hycast::ChunkInfo chunkInfo;
    public:
        ClientMsgRcvr(
                const hycast::ProdInfo prodInfo,
                const hycast::ChunkInfo chunkInfo)
            : prodInfo{prodInfo}
            , chunkInfo{chunkInfo}
        {}
        void recvNotice(const hycast::ProdInfo& info, const hycast::Peer& peer) {
            EXPECT_EQ(prodInfo, info);
        }
        void recvNotice(const hycast::ChunkInfo& info, const hycast::Peer& peer) {
            EXPECT_EQ(chunkInfo, info);
        }
        void recvRequest(const hycast::ProdIndex& index, const hycast::Peer& peer) {
        }
        void recvRequest(const hycast::ChunkInfo& info, const hycast::Peer& peer) {
        }
        void recvData(hycast::LatentChunk chunk, const hycast::Peer& peer) {
            chunk.discard();
        }
    };

    /**
     * Server-side message-receiver that's a black hole.
     */
    class ServerMsgRcvr final : public hycast::PeerMsgRcvr
    {
    public:
        void recvNotice(const hycast::ProdInfo& info, const hycast::Peer& peer)
        {}
        void recvNotice(const hycast::ChunkInfo& info, const hycast::Peer& peer)
        {}
        void recvRequest(const hycast::ProdIndex& index, const hycast::Peer& peer)
        {}
        void recvRequest(const hycast::ChunkInfo& info, const hycast::Peer& peer)
        {}
        void recvData(hycast::LatentChunk latentChunk, const hycast::Peer& peer)
        {
                latentChunk.discard();
        }
    };

    /**
     * Server that is a black hole.
     */
    class Server {
        std::thread thread;
        void runServer(hycast::SrvrSctpSock serverSock) {
            try {
                ServerMsgRcvr     srvrMsgRcvr{};
                hycast::ProdStore prodStore{};
                hycast::PeerSet   peerSet{prodStore, srvrMsgRcvr, 2};
                for (;;) {
                    auto sock = serverSock.accept();
                    try {
                        hycast::Peer peer{sock};
                        peerSet.tryInsert(peer);
                    }
                    catch (const std::exception& ex) {
                        LOG_WARN(ex, "Couldn't accept remote peer");
                    }
                }
            }
            catch (const std::exception& ex) {
                LOG_ERROR(ex, "Server threw an exception");
            }
        }
    public:
        Server(const hycast::InetSockAddr& serverSockAddr)
            : thread{}
        {
            hycast::SrvrSctpSock serverSock{serverSockAddr,
                hycast::Peer::getNumStreams()};
            serverSock.listen();
            thread = std::thread([this,serverSock]{runServer(serverSock);});
        }
        ~Server() {
            if (thread.joinable()) {
                auto status = ::pthread_cancel(thread.native_handle());
                if (status)
                    hycast::log_error(hycast::SYSTEM_ERROR(
                            "Couldn't cancel server thread", status));
                thread.join();
            }
        }
    };

    PeerSetTest()
        : server1SockAddr{"127.0.0.1", 38800}
        , server2SockAddr{hycast::Interface{ETHNET_IFACE_NAME}.getInetAddr(AF_INET),
                38800}
        , prodStore{}
    {}

    hycast::Peer getClientPeer(const hycast::InetSockAddr& serverSockAddr) {
        hycast::SctpSock sock{hycast::Peer::getNumStreams()};
        sock.connect(serverSockAddr);
        return hycast::Peer{sock};
    }

    // Objects declared here can be used in all TEST_F tests
    const hycast::InetSockAddr server1SockAddr;
    const hycast::InetSockAddr server2SockAddr;
    const hycast::ProdInfo     prodInfo{"product", 1, 100000};
    const hycast::ChunkInfo    chunkInfo{prodInfo, 2};
    ClientMsgRcvr              clntMsgRcvr{prodInfo, chunkInfo};
    hycast::ProdStore          prodStore;
};

// Tests default construction
TEST_F(PeerSetTest, DefaultConstruction) {
    ClientMsgRcvr msgRcvr{prodInfo, chunkInfo};
    hycast::PeerSet peerSet{prodStore, msgRcvr, 1}; // 1 second maximum residence
}

// Tests construction with invalid argument
TEST_F(PeerSetTest, InvalidConstruction) {
    ClientMsgRcvr msgRcvr{prodInfo, chunkInfo};
    EXPECT_THROW(hycast::PeerSet(prodStore, msgRcvr, 2, 0), std::invalid_argument);
}

// Tests inserting a peer and incrementing its value
TEST_F(PeerSetTest, IncrementPeerValue) {
    try {
        //std::set_terminate([]{std::cerr << "In terminate()\n"; ::pause();});
        Server           server{server1SockAddr};
        ClientMsgRcvr    msgRcvr{prodInfo, chunkInfo};
        hycast::PeerSet  peerSet{prodStore, msgRcvr, 2};
        auto             peer = getClientPeer(server1SockAddr);
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
        Server           server1{server1SockAddr};
        ClientMsgRcvr    msgRcvr{prodInfo, chunkInfo};
        hycast::PeerSet  peerSet{prodStore, msgRcvr, 0, 1};

        hycast::Peer peer1{getClientPeer(server1SockAddr)};
        EXPECT_TRUE(peerSet.tryInsert(peer1));

        Server       server2{server2SockAddr};
        hycast::Peer peer2{getClientPeer(server2SockAddr)};
        EXPECT_TRUE(peerSet.tryInsert(peer2));

        EXPECT_EQ(1, peerSet.size());
    }
    catch (const std::exception& e) {
        hycast::log_error(e);
    }
}

// Tests inserting a peer and sending notices
TEST_F(PeerSetTest, PeerInsertionAndNotices) {
    Server           server{server1SockAddr};
    hycast::Peer     peer{getClientPeer(server1SockAddr)};
    ClientMsgRcvr    msgRcvr{prodInfo, chunkInfo};
    hycast::PeerSet  peerSet{prodStore, msgRcvr, 2};
    EXPECT_TRUE(peerSet.tryInsert(peer));
    peerSet.sendNotice(prodInfo);
    peerSet.sendNotice(chunkInfo);
    ::usleep(100000);
}

// Tests inserting the same peer twice
TEST_F(PeerSetTest, DuplicatePeerInsertion) {
    Server server{server1SockAddr};
    hycast::Peer     peer{getClientPeer(server1SockAddr)};
    ClientMsgRcvr    msgRcvr{prodInfo, chunkInfo};
    hycast::PeerSet  peerSet{prodStore, msgRcvr, 2};
    EXPECT_TRUE(peerSet.tryInsert(peer));
    EXPECT_FALSE(peerSet.tryInsert(peer));
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
