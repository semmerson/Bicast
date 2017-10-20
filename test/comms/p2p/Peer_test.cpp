/**
 * This file tests the class `Peer`.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Peer_test.cpp
 * @author: Steven R. Emmerson
 */

#include "ChunkInfo.h"
#include "Completer.h"
#include "error.h"
#include "HycastTypes.h"
#include "InetSockAddr.h"
#include "ProdInfo.h"
#include "SctpSock.h"
#include "Thread.h"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <ctime>
#include <functional>
#include <gtest/gtest.h>
#include <p2p/Peer.h>
#include <p2p/PeerMsgRcvr.h>
#include <iostream>
#include <mutex>
#include <ratio>
#include <thread>

namespace {

static hycast::InetSockAddr srvrSockAddr;

// The fixture for testing class Peer.
class PeerTest : public ::testing::Test {
friend class TestPeerMgr;
protected:
    typedef std::chrono::high_resolution_clock Clock;
    typedef Clock::time_point                  TimePoint;
    typedef std::chrono::duration<double>      Duration;

    PeerTest()
        : data{new char[hycast::ChunkInfo::getCanonSize()]}
    {
        ::memset(data, 0xbd, hycast::ChunkInfo::getCanonSize());
    }

    virtual ~PeerTest()
    {
        delete[] data;
    }

    void runTestSrvr(hycast::SrvrSctpSock& srvrSock)
    {
        try {
            auto sock = srvrSock.accept();
            hycast::Peer peer{sock};

            LOG_INFO("Sending product notice");
            peer.sendNotice(prodInfo);
            LOG_INFO("Sending chunk notice");
            peer.sendNotice(chunkInfo);
            LOG_INFO("Sending product request");
            peer.sendRequest(prodIndex);
            LOG_INFO("Sending chunk request");
            peer.sendRequest(chunkInfo);
            hycast::ActualChunk actualChunk(chunkInfo, data);
            LOG_INFO("Sending chunk");
            peer.sendData(actualChunk);
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(hycast::RUNTIME_ERROR(
                    "runTestSrvr() threw an exception"));
        }
    }

    void startTestSrvr()
    {
        // Server socket must exist before client connects
        hycast::SrvrSctpSock srvrSock{srvrSockAddr,
                hycast::Peer::getNumStreams()};
        srvrSock.listen();
        senderFuture = completer.submit([this,srvrSock]() mutable {
            this->runTestSrvr(srvrSock); });
    }

    void runTestClnt(hycast::SctpSock& sock)
    {
        try {
            hycast::Peer peer{sock};

            auto msg = peer.getMessage();
            EXPECT_EQ(peer.PROD_NOTICE, msg.getType());
            EXPECT_EQ(prodInfo, msg.getProdInfo());
            LOG_INFO("Received product notice");

            msg = peer.getMessage();
            EXPECT_EQ(peer.CHUNK_NOTICE, msg.getType());
            EXPECT_EQ(chunkInfo, msg.getChunkInfo());
            LOG_INFO("Received chunk notice");

            msg = peer.getMessage();
            EXPECT_EQ(peer.PROD_REQUEST, msg.getType());
            EXPECT_EQ(prodIndex, msg.getProdIndex());
            LOG_INFO("Received product request");

            msg = peer.getMessage();
            EXPECT_EQ(peer.CHUNK_REQUEST, msg.getType());
            EXPECT_EQ(chunkInfo, msg.getChunkInfo());
            LOG_INFO("Received chunk request");

            msg = peer.getMessage();
            EXPECT_EQ(peer.CHUNK, msg.getType());
            auto chunk = msg.getChunk();
            EXPECT_EQ(chunkInfo, chunk.getInfo());
            char data[chunk.getSize()];
            auto actualSize = chunk.drainData(data, sizeof(data));
            ASSERT_EQ(sizeof(data), actualSize);
            EXPECT_EQ(0, ::memcmp(this->data, data, sizeof(data)));
            LOG_INFO("Received chunk");
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(hycast::RUNTIME_ERROR(
                    "runTestClnt() threw an exception"));
        }
    }

    void startTestClnt()
    {
        hycast::SctpSock sock{hycast::Peer::getNumStreams()};
        sock.connect(srvrSockAddr);
        receiverFuture = completer.submit([this,sock]() mutable {
            this->runTestClnt(sock); });
    }

    void runPerfSrvr(hycast::SrvrSctpSock& srvrSock)
    {
        try {
            auto             sock = srvrSock.accept();
            hycast::Peer     peer(sock);
            const size_t     dataSize = 1000000;
            hycast::ProdInfo prodInfo("product", 0, dataSize);
            char             data[hycast::chunkSizeMax];
            ::memset(data, 0xbd, sizeof(data));
            for (hycast::ChunkSize chunkSize = hycast::chunkSizeMax - 8;
                    chunkSize > 1000; chunkSize /= 2) {
                hycast::ChunkInfo::setCanonSize(chunkSize);
                TimePoint start = Clock::now();
                peer.sendNotice(prodInfo);
                size_t remaining = dataSize;
                auto numChunks = prodInfo.getNumChunks();
                for (hycast::ChunkIndex chunkIndex = 0; chunkIndex < numChunks;
                        ++chunkIndex) {
                    try {
                        hycast::ActualChunk chunk{
                                hycast::ChunkInfo{prodInfo, chunkIndex}, data};
                        peer.sendData(chunk);
                        remaining -= chunk.getSize();
                    }
                    catch (const std::exception& ex) {
                        std::throw_with_nested(hycast::RUNTIME_ERROR(
                                "Couldn't send chunk"));
                    }
                }
#if 0
                std::chrono::high_resolution_clock::time_point stop =
                        std::chrono::high_resolution_clock::now();
                std::chrono::duration<double> time_span =
                        std::chrono::duration_cast<
                        std::chrono::duration<double>>(stop - start);
                double seconds = time_span.count()*
                        decltype(time_span)::period::num/
                        decltype(time_span)::period::den;
                std::cerr << "Chunk size=" + std::to_string(chunkSize) +
                        " bytes, duration=" + std::to_string(seconds) +
                        " s, byte rate=" + std::to_string(dataSize/seconds) +
                        " Hz" << std::endl;
#endif
            }
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(
                    hycast::RUNTIME_ERROR("runPerfSrvr() threw an exception"));
        }
    }

    void startPerfSrvr()
    {
        // Server socket must exist before client connects
        hycast::SrvrSctpSock srvrSock{srvrSockAddr,
                hycast::Peer::getNumStreams()};
        srvrSock.listen();
        senderFuture = completer.submit([this,srvrSock]() mutable {
            this->runPerfSrvr(srvrSock); });
    }

    void runPerfClnt(hycast::SctpSock& sock)
    {
        try {
            hycast::Peer peer{sock};
            for (;;) {
                // Get product information
                auto msg = peer.getMessage();
                if (!msg)
                    break;
                auto start = Clock::now();
                hycast::ProdInfo prodInfo{};
                try {
                    prodInfo = msg.getProdInfo();
                }
                catch (const std::exception& ex) {
                    std::throw_with_nested(hycast::RUNTIME_ERROR(
                            "Couldn't get product-info"));
                }
                auto prodSize = prodInfo.getSize();
                // Get first chunk and canonical chunk size
                hycast::LatentChunk chunk{};
                try {
                    chunk = peer.getMessage().getChunk();
                    chunk.discard();
                }
                catch (const std::exception& ex) {
                    std::throw_with_nested(hycast::RUNTIME_ERROR(
                            "Couldn't get first chunk"));
                }
                auto canonChunkSize = chunk.getSize();
                // Get remaining chunks
                auto lastChunkIndex = (prodSize + canonChunkSize - 1)
                            / canonChunkSize - 1;
                while (chunk.getIndex() < lastChunkIndex) {
                    try {
                        chunk = peer.getMessage().getChunk();
                        chunk.discard();
                    }
                    catch (const std::exception& ex) {
                        std::throw_with_nested(hycast::RUNTIME_ERROR(
                                "Couldn't get chunk " +
                                std::to_string(chunk.getIndex()+1)));
                    }
                }
                TimePoint stop = Clock::now();
                Duration  duration = std::chrono::duration_cast<Duration>
                        (stop - start);
                auto      seconds = duration.count() *
                        Duration::period::num / Duration::period::den;
                std::cerr << "Chunk size=" + std::to_string(canonChunkSize)
                        + " bytes, duration=" + std::to_string(seconds) +
                        " s, byte rate=" + std::to_string(prodSize/seconds)
                        + " Hz" << std::endl;
            }
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(
                    hycast::RUNTIME_ERROR("runPerfClnt() threw an exception"));
        }
    }

    void startPerfClnt()
    {
        hycast::SctpSock sock{hycast::Peer::getNumStreams()};
        sock.connect(srvrSockAddr);
        receiverFuture = completer.submit([this,sock]() mutable {
            this->runPerfClnt(sock); });
    }

    // Objects declared here can be used by all tests in the test case for Peer.
    hycast::Future<void>    senderFuture;
    hycast::Future<void>    receiverFuture;
    hycast::ProdInfo        prodInfo{"product", 1, 100000};
    hycast::ChunkInfo       chunkInfo{prodInfo, 3};
    hycast::ProdIndex       prodIndex{1};
    char*                   data;
    hycast::Completer<void> completer{};
};

// Tests default construction
TEST_F(PeerTest, DefaultConstruction) {
    hycast::Peer peer{};
}

// Tests to_string
TEST_F(PeerTest, ToString) {
    EXPECT_STREQ("{addr=:0, version=0, sock={sd=-1, numStreams=0}}",
            hycast::Peer().to_string().data());
}

// Tests transmission
TEST_F(PeerTest, Transmission) {
    try {
        startTestSrvr();
        startTestClnt();
        completer.take().getResult();
        completer.take().getResult();
    }
    catch (const std::exception& ex) {
        LOG_ERROR(ex, "Transmission test failure"); // Else no message nesting
        throw;
    }
}

#if 1
// Tests performance
TEST_F(PeerTest, Performance) {
    try {
        startPerfSrvr();
        startPerfClnt();
        completer.take().getResult();
        completer.take().getResult();
    }
    catch (const std::exception& ex) {
        LOG_ERROR(ex, "Performance test failure"); // Else no message nesting
        throw;
    }
}
#endif

}  // namespace

int main(int argc, char **argv) {
    hycast::logLevel = hycast::LogLevel::LOG_INFO;
    const char* serverIpAddrStr = "127.0.0.1";
    if (argc > 1)
        serverIpAddrStr = argv[1];
    srvrSockAddr = hycast::InetSockAddr{serverIpAddrStr, 38800};
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
