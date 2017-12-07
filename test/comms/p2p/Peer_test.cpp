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

#include "Completer.h"
#include "error.h"
#include "Peer.h"
#include "PeerServer.h"
#include "SctpSock.h"
#include "Thread.h"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <ctime>
#include <functional>
#include <gtest/gtest.h>
#include <iostream>
#include <mutex>
#include <ratio>
#include <thread>

namespace {

static hycast::InetSockAddr srvrSockAddr;

class TransmitProd : public hycast::PeerServer
{
    typedef std::chrono::high_resolution_clock Clock;
    typedef Clock::time_point                  TimePoint;
    typedef std::chrono::duration<double>      Duration;

    hycast::Future<void>       senderFuture;
    hycast::Future<void>       receiverFuture;
    const hycast::ProdIndex    prodIndex;
    const hycast::ProdName     prodName;
    const hycast::ProdSize     prodSize;
    const hycast::ChunkSize    chunkSize;
    const hycast::ProdInfo     prodInfo;
    const hycast::ChunkIndex   numChunks;
    const hycast::ChunkId      backlogChunkId;
    char*                      chunkData;
    hycast::Barrier            barrier;
    hycast::ChunkIndex::type   numRcvdChunks;

public:
    TransmitProd(const hycast::ChunkSize::type chunkSize)
        : senderFuture{}
        , receiverFuture{}
        , prodIndex{1}
        , prodName{"product"}
        , prodSize{1000000}
        , chunkSize{chunkSize}
        , prodInfo{prodIndex, prodName, prodSize, chunkSize}
        , numChunks{prodInfo.getNumChunks()}
        , backlogChunkId{prodInfo, 0}
        , chunkData{new char[chunkSize]}
        , barrier{3}
        , numRcvdChunks{0}
    {
        ::memset(chunkData, 0xbd, chunkSize);
    }

    ~TransmitProd()
    {
        delete[] chunkData;
    }

    void startBacklog(const hycast::ChunkId& chunkId) {
        EXPECT_EQ(backlogChunkId, chunkId);
    }
    bool shouldRequest(const hycast::ProdIndex& index) {
        EXPECT_EQ(prodIndex, index);
        return true;
    }
    bool shouldRequest(const hycast::ChunkId& chunkId) {
        EXPECT_EQ(prodIndex, chunkId.getProdIndex());
        EXPECT_GT(numChunks, chunkId.getChunkIndex());
        return true;
    }
    bool get(const hycast::ProdIndex& index, hycast::ProdInfo& info) {
        EXPECT_EQ(prodIndex, index);
        info = prodInfo;
        return true;
    }
    bool get(const hycast::ChunkId& chunkId, hycast::ActualChunk& chunk) {
        EXPECT_EQ(prodIndex, chunkId.getProdIndex());
        auto chunkIndex = chunkId.getChunkIndex();
        EXPECT_GT(numChunks, chunkIndex);
        const hycast::ChunkInfo chunkInfo{prodInfo, chunkIndex};
        chunk = hycast::ActualChunk(chunkInfo, chunkData);
        return true;
    }
    hycast::RecvStatus receive(const hycast::ProdInfo& info) {
        EXPECT_EQ(prodInfo, info);
        return hycast::RecvStatus{};
    }
    hycast::RecvStatus receive(hycast::LatentChunk& chunk) {
        auto chunkIndex = chunk.getIndex();
        EXPECT_GT(numChunks, chunkIndex);
        EXPECT_EQ(prodIndex, chunk.getProdIndex());
        EXPECT_EQ(prodSize, chunk.getProdSize());
        EXPECT_EQ(chunkSize, chunk.getCanonSize());
        auto chunkSize = chunk.getSize();
        EXPECT_EQ(prodInfo.getChunkSize(chunkIndex), chunkSize);
        char data[static_cast<size_t>(chunkSize)];
        chunk.drainData(data, chunkSize);
        //EXPECT_EQ(0, ::memcmp(chunkData, data, chunkSize)); // Most time here
        if (++numRcvdChunks == numChunks)
            barrier.wait();
        return hycast::RecvStatus{};
    }

    void runSrvr(hycast::SrvrSctpSock& srvrSock)
    {
        try {
            auto              sock = srvrSock.accept();
            hycast::Peer      peer(sock);
            hycast::Thread    recvThread{[&peer,this]() mutable {
                    peer.runReceiver(*this);}};
            peer.notify(prodIndex);
            for (hycast::ChunkIndex chunkIndex = 0;
                    chunkIndex < numChunks; ++chunkIndex) {
                try {
                    hycast::ChunkId chunkId{prodInfo, chunkIndex};
                    peer.notify(chunkId);
                }
                catch (const std::exception& ex) {
                    std::throw_with_nested(hycast::RUNTIME_ERROR(
                            "Couldn't send chunk " + chunkIndex.to_string()));
                }
            }
            barrier.wait();
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(
                    hycast::RUNTIME_ERROR("runSrvr() threw an exception"));
        }
    }

    void startSrvr(hycast::Completer<void>& completer)
    {
        // Server socket must exist before client connects
        hycast::SrvrSctpSock srvrSock{srvrSockAddr,
                hycast::Peer::getNumStreams()};
        srvrSock.listen();
        senderFuture = completer.submit([this,srvrSock]() mutable {
            this->runSrvr(srvrSock); });
    }

    void runClnt()
    {
        try {
            hycast::SctpSock sock{hycast::Peer::getNumStreams()};
            sock.connect(srvrSockAddr);
            hycast::Peer   peer{sock};
            auto           start = Clock::now();
            hycast::Thread recvThread{[&peer,this]() mutable {
                    peer.runReceiver(*this);}};
            barrier.wait();
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
        } // Destroys `recvThread`
        catch (const std::exception& ex) {
            std::throw_with_nested(
                    hycast::RUNTIME_ERROR("runClnt() threw an exception"));
        }
    }

    void startClnt(hycast::Completer<void>& completer)
    {
        receiverFuture = completer.submit([this]() mutable {
            this->runClnt(); });
    }
};

// The fixture for testing class Peer.
class PeerTest : public ::testing::Test
{};

// Tests default construction
TEST_F(PeerTest, DefaultConstruction) {
    hycast::Peer peer{};
}

// Tests to_string
TEST_F(PeerTest, ToString) {
    EXPECT_STREQ("{addr=:0, version=0, sock={sd=-1, numStreams=0}}",
            hycast::Peer().to_string().data());
}

// Tests execution
TEST_F(PeerTest, Execution) {
    hycast::ChunkSize::type chunkSizes[6] = { 8000, 16000, 24000, 32000, 40000,
            48000};
    try {
        for (auto chunkSize : chunkSizes) {
            TransmitProd transProd{chunkSize};
            hycast::Completer<void> completer{};
            transProd.startSrvr(completer);
            transProd.startClnt(completer);
            completer.take().getResult();
            completer.take().getResult();
        }
    }
    catch (const std::exception& ex) {
        LOG_ERROR(ex, "Execution test failure"); // Else no message nesting
        throw;
    }
}

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
