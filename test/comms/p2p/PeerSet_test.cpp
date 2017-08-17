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

#include "error.h"
#include "HycastTypes.h"
#include "Interface.h"
#include "logging.h"
#include "PeerMsgRcvr.h"
#include "PeerSet.h"
#include "ProdInfo.h"
#include "SctpSock.h"
#include "SrvrSctpSock.h"

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
        hycast::ProdInfo prodInfo;
        hycast::ChunkInfo chunkInfo;
    public:
        ClientMsgRcvr(
                const hycast::ProdInfo prodInfo,
                const hycast::ChunkInfo chunkInfo)
            : prodInfo{prodInfo}
            , chunkInfo{chunkInfo}
        {}
        void recvNotice(const hycast::ProdInfo& info) {}
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
        void recvData(hycast::LatentChunk chunk) {}
        void recvData(hycast::LatentChunk chunk, const hycast::Peer& peer) {
            chunk.discard();
        }
    };

    /**
     * Server that is a black hole.
     */
    class Server {
        /**
         * Thread-safe peer-manager that echos everything back to the remote
         * peer.
         */
        class ServerMsgRcvr final : public hycast::PeerMsgRcvr {
            hycast::ProdInfo prodInfo;
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
        std::thread thread;
        void runServer(hycast::SrvrSctpSock serverSock) {
            ServerMsgRcvr srvrMsgRcvr{};
            hycast::PeerSet peerSet{2};
            for (;;) {
                try {
                    ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, nullptr);
                    hycast::SctpSock sock{serverSock.accept()};
                    ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, nullptr);
                    hycast::Peer peer{srvrMsgRcvr, sock};
                    peerSet.tryInsert(peer);
                }
                catch (const std::exception& e) {
                    hycast::log_what(e);
                }
            }
        }
    public:
        Server(const hycast::InetSockAddr& serverSockAddr)
            : thread{}
        {
            hycast::SrvrSctpSock serverSock{serverSockAddr,
                hycast::Peer::getNumStreams()};
            thread = std::thread([this,serverSock]{runServer(serverSock);});
        }
        ~Server() {
            if (thread.joinable()) {
                auto status = ::pthread_cancel(thread.native_handle());
                if (status)
                    hycast::log_what(hycast::SystemError(__FILE__, __LINE__,
                            "Couldn't cancel server thread", status));
                thread.join();
            }
        }
    };

    PeerSetTest()
        : server1SockAddr{"127.0.0.1", 38800}
        , server2SockAddr{hycast::Interface{"ens33"}.getInetAddr(AF_INET),
                38800}
    {}

    hycast::Peer getClientPeer(const hycast::InetSockAddr& serverSockAddr) {
        hycast::SctpSock sock{serverSockAddr, hycast::Peer::getNumStreams()};
        return hycast::Peer(clntMsgRcvr, sock);
    }

    // Objects declared here can be used in all TEST_F tests
    const hycast::InetSockAddr server1SockAddr;
    const hycast::InetSockAddr server2SockAddr;
    const hycast::ProdInfo     prodInfo{"product", 1, 100000};
    const hycast::ChunkInfo    chunkInfo{prodInfo, 2};
    ClientMsgRcvr              clntMsgRcvr{prodInfo, chunkInfo};
};

// Tests default construction
TEST_F(PeerSetTest, DefaultConstruction) {
    hycast::PeerSet peerSet{2};
}

// Tests construction with invalid argument
TEST_F(PeerSetTest, InvalidConstruction) {
    EXPECT_THROW(hycast::PeerSet(2, 0), std::invalid_argument);
}

// Tests inserting a peer and incrementing its value
TEST_F(PeerSetTest, IncrementPeerValue) {
    try {
        //std::set_terminate([]{std::cerr << "In terminate()\n"; ::pause();});
        Server server{server1SockAddr};
        hycast::Peer     peer{getClientPeer(server1SockAddr)};
        hycast::PeerSet  peerSet{2};
        EXPECT_TRUE(peerSet.tryInsert(peer));
        EXPECT_EQ(1, peerSet.size());
        EXPECT_NO_THROW(peerSet.incValue(peer));
    }
    catch (const std::exception& e) {
        hycast::log_what(e);
    }
}

// Tests removing the worst peer from a 1-peer set
TEST_F(PeerSetTest, RemoveWorst) {
    try {
        hycast::PeerSet  peerSet{0, 1};

        Server server1{server1SockAddr};
        hycast::Peer peer1{getClientPeer(server1SockAddr)};
        EXPECT_TRUE(peerSet.tryInsert(peer1));

        Server server2{server2SockAddr};
        hycast::Peer peer2{getClientPeer(server2SockAddr)};
        EXPECT_TRUE(peerSet.tryInsert(peer2));

        EXPECT_EQ(1, peerSet.size());
    }
    catch (const std::exception& e) {
        hycast::log_what(e);
    }
}

// Tests inserting a peer and sending notices
TEST_F(PeerSetTest, PeerInsertionAndNotices) {
    Server server{server1SockAddr};
    hycast::Peer     peer{getClientPeer(server1SockAddr)};
    hycast::PeerSet  peerSet{2};
    EXPECT_TRUE(peerSet.tryInsert(peer));
    peerSet.sendNotice(prodInfo);
    peerSet.sendNotice(chunkInfo);
    ::usleep(100000);
}

// Tests inserting the same peer twice
TEST_F(PeerSetTest, DuplicatePeerInsertion) {
    Server server{server1SockAddr};
    hycast::Peer     peer{getClientPeer(server1SockAddr)};
    hycast::PeerSet  peerSet{2};
    EXPECT_TRUE(peerSet.tryInsert(peer));
    EXPECT_FALSE(peerSet.tryInsert(peer));
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
