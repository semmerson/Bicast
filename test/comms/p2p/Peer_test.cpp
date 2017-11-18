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

#include <Chunk.h>
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
        : data{new char[hycast::ChunkSize::defaultChunkSize]}
        , barrier{2}
    {
        ::memset(data, 0xbd, hycast::ChunkSize::defaultChunkSize);
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
            peer.sendNotice(chunkInfo.getId());
            LOG_INFO("Sending product request");
            peer.sendRequest(prodIndex);
            LOG_INFO("Sending chunk request");
            peer.sendRequest(chunkInfo.getId());
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
            EXPECT_EQ(chunkInfo, msg.getChunkId());
            LOG_INFO("Received chunk notice");

            msg = peer.getMessage();
            EXPECT_EQ(peer.PROD_REQUEST, msg.getType());
            EXPECT_EQ(prodIndex, msg.getProdIndex());
            LOG_INFO("Received product request");

            msg = peer.getMessage();
            EXPECT_EQ(peer.CHUNK_REQUEST, msg.getType());
            EXPECT_EQ(chunkInfo, msg.getChunkId());
            LOG_INFO("Received chunk request");

            msg = peer.getMessage();
            EXPECT_EQ(peer.CHUNK, msg.getType());
            auto chunk = msg.getChunk();
            EXPECT_TRUE(chunkInfo.equalsExceptName(chunk.getInfo()));
            char data[static_cast<size_t>(chunk.getSize())];
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
            char             chunkData[hycast::ChunkSize::chunkSizeMax];
            const size_t     prodSize = 1000000;
            ::memset(chunkData, 0xbd, sizeof(chunkData));
            for (hycast::ChunkSize chunkSize : chunkSizes) {
                {
                    auto             sock = srvrSock.accept();
                    hycast::Peer     peer(sock);
                    hycast::ProdInfo prodInfo{0, "product", prodSize,
                            chunkSize};
                    TimePoint start = Clock::now();
                    peer.sendNotice(prodInfo);
                    size_t remaining = prodSize;
                    auto numChunks = prodInfo.getNumChunks();
                    for (hycast::ChunkIndex chunkIndex = 0;
                            chunkIndex < numChunks; ++chunkIndex) {
                        try {
                            hycast::ActualChunk chunk{
                                    hycast::ChunkInfo{prodInfo, chunkIndex},
                                    chunkData};
                            peer.sendData(chunk);
                            remaining -= chunk.getSize();
                        }
                        catch (const std::exception& ex) {
                            std::throw_with_nested(hycast::RUNTIME_ERROR(
                                    "Couldn't send chunk"));
                        }
                    }
                } // Connection closed
                barrier.wait();
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

    void runPerfClnt()
    {
        try {
            for (hycast::ChunkSize chunkSize : chunkSizes) {
                hycast::SctpSock sock{hycast::Peer::getNumStreams()};
                sock.connect(srvrSockAddr);
                hycast::Peer peer{sock};
                auto start = Clock::now();
                // Receive product information
                prodInfo = peer.getMessage().getProdInfo();
                auto prodSize = prodInfo.getSize();
                // Receive data-chunks
                for (auto msg = peer.getMessage(); msg;
                        msg = peer.getMessage()) {
                    msg.getChunk().discard();
                }
                TimePoint stop = Clock::now();
                Duration  duration = std::chrono::duration_cast<Duration>
                        (stop - start);
                auto seconds = duration.count() *
                        Duration::period::num / Duration::period::den;
                std::cerr << "Chunk size=" +
                        std::to_string(chunkSize)
                        + " bytes, duration=" + std::to_string(seconds) +
                        " s, byte rate=" + std::to_string(prodSize/seconds)
                        + " Hz" << std::endl;
                barrier.wait();
            }
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(
                    hycast::RUNTIME_ERROR("runPerfClnt() threw an exception"));
        }
    }

    void startPerfClnt()
    {
        receiverFuture = completer.submit([this]() mutable {
            this->runPerfClnt(); });
    }

    // Objects declared here can be used by all tests in the test case for Peer.
    hycast::Future<void>    senderFuture;
    hycast::Future<void>    receiverFuture;
    hycast::ProdInfo        prodInfo{1, "product", 100000};
    hycast::ChunkInfo       chunkInfo{prodInfo, 3};
    hycast::ProdIndex       prodIndex{1};
    char*                   data;
    hycast::Completer<void> completer{};
    hycast::ChunkSize       chunkSizes[6] = {8000, 16000, 24000, 32000, 40000, 48000};
    hycast::Barrier         barrier;
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
