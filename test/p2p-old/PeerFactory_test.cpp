/**
 * This file tests class `PeerFactory`.
 *
 *       File: PeetFactory_test.cpp
 * Created On: June 6, 2019
 *     Author: Steven R. Emmerson
 *
 *    Copyright 2021 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "config.h"

#include "error.h"
#include "SockAddr.h"

#include <condition_variable>
#include <gtest/gtest.h>
#include <main/p2p-old/PeerFactory.h>
#include <mutex>
#include <thread>

namespace {

/// The fixture for testing class `PeerFactory`
class PeerFactoryTest : public ::testing::Test, public hycast::XcvrPeerMgr
{
protected:
    friend class Receiver;

    typedef enum {
        INIT = 0,
        PUB_PEER_CREATED = 0x1,
        PROD_NOTICE_RCVD = 0x2,
        SEG_NOTICE_RCVD = 0x4,
        PROD_REQUEST_RCVD = 0x8,
        SEG_REQUEST_RCVD = 0x10,
        PROD_INFO_RCVD = 0x20,
        SEG_RCVD = 0x40,
        DONE = PUB_PEER_CREATED |
               PROD_NOTICE_RCVD |
               SEG_NOTICE_RCVD |
               PROD_REQUEST_RCVD |
               SEG_REQUEST_RCVD |
               PROD_INFO_RCVD |
               SEG_RCVD
    } State;
    State                   state;
    hycast::SockAddr        pubAddr;
    hycast::SockAddr        subAddr;
    std::mutex              mutex;
    std::condition_variable cond;
    hycast::ProdIndex       prodIndex;
    hycast::ProdSize        prodSize;
    hycast::SegSize         segSize;
    hycast::ProdInfo        prodInfo;
    hycast::SegId           segId;
    hycast::SegInfo         segInfo;
    char                    memData[1000];
    hycast::MemSeg          memSeg;
    hycast::Peer            pubPeer;

    // You can remove any or all of the following functions if its body
    // is empty.

    PeerFactoryTest()
        : state{INIT}
        , pubAddr{"localhost:3880"}
        , subAddr{"localhost:3881"}
        , mutex{}
        , cond{}
        , prodIndex{1}
        , prodSize{1000000}
        , segSize{sizeof(memData)}
        , prodInfo{prodIndex, prodSize, "product"}
        , segId(prodIndex, segSize)
        , segInfo(segId, prodSize, segSize)
        , memData{}
        , memSeg{segInfo, memData}
        , pubPeer()
    {
        ::memset(memData, 0xbd, segSize);
    }

public:
    void orState(const State state)
    {
        std::lock_guard<decltype(mutex)> guard{mutex};
        this->state = static_cast<State>(this->state | state);
        cond.notify_all();
    }

    void waitForState(const State state)
    {
        std::unique_lock<decltype(mutex)> lock{mutex};
        while (this->state != state)
            cond.wait(lock);
    }

    void pathToPub(const hycast::SockAddr& rmtAddr)
    {}

    void noPathToPub(const hycast::SockAddr& rmtAddr)
    {}

    // Receiver-side
    bool shouldRequest(
            const hycast::SockAddr& rmtAddr,
            const hycast::ProdIndex actual)
    {
        EXPECT_TRUE(prodIndex == actual);
        orState(PROD_NOTICE_RCVD);

        return true;
    }

    // Receiver-side
    bool shouldRequest(
            const hycast::SockAddr& rmtAddr,
            const hycast::SegId&    actual)
    {
        EXPECT_EQ(segId, actual);
        orState(SEG_NOTICE_RCVD);

        return true;
    }

    // Sender-side
    hycast::ProdInfo getProdInfo(
            const hycast::SockAddr& remote,
            const hycast::ProdIndex actual)
    {
        EXPECT_TRUE(prodIndex == actual);
        orState(PROD_REQUEST_RCVD);
        return prodInfo;
    }

    // Sender-side
    hycast::MemSeg getMemSeg(
            const hycast::SockAddr& remote,
            const hycast::SegId&    actual)
    {
        EXPECT_EQ(segId, actual);
        orState(SEG_REQUEST_RCVD);
        return memSeg;
    }

    // Receiver-side
    bool hereIs(
            const hycast::SockAddr& rmtAddr,
            const hycast::ProdInfo& actual)
    {
        EXPECT_EQ(prodInfo, actual);
        orState(PROD_INFO_RCVD);

        return true;
    }

    // Receiver-side
    bool hereIs(
            const hycast::SockAddr& rmtAddr,
            hycast::TcpSeg&         actual)
    {
        const hycast::SegSize size = actual.getSegInfo().getSegSize();
        EXPECT_EQ(segSize, size);

        char buf[size];
        actual.getData(buf);

        EXPECT_EQ(0, ::memcmp(memSeg.data(), buf, segSize));

        orState(SEG_RCVD);

        return true;
    }

    void runPub(hycast::PubPeerFactory& factory)
    {
        try {
            pubPeer = factory.accept();
            if (pubPeer) {
                orState(PUB_PEER_CREATED);
                pubPeer();
            }
        }
        catch (const std::exception& ex) {
            hycast::log_error(ex);
        }
        catch (...) {
            LOG_NOTE("Server thread cancelled");
        }
    }
};

// Tests closing the factory
TEST_F(PeerFactoryTest, FactoryClosure)
{
    // Start a server. Calls `::listen()`.
    hycast::PubPeerFactory factory(pubAddr, 1, *this);
    std::thread            pubThread(&PeerFactoryTest::runPub, this,
            std::ref(factory));

    try {
        // Close the factory. Causes `runPub()` to return.
        factory.close();

        pubThread.join();
    }
    catch (const std::exception& ex) {
        hycast::log_error(ex);
        pubThread.join();
        throw;
    }
}

// Tests complete exchange (notice, request, delivery)
TEST_F(PeerFactoryTest, Exchange)
{
    // Start a publisher. Calls `::listen()`.
    hycast::PubPeerFactory pubFactory(pubAddr, 1, *this);
    std::thread         pubThread(&PeerFactoryTest::runPub, this,
            std::ref(pubFactory));

    // Start a subscriber
    hycast::SubPeerFactory subFactory(subAddr, 1, *this);
    hycast::Peer subPeer = subFactory.connect(pubAddr,
            hycast::NodeType::NO_PATH_TO_PUBLISHER);
    std::thread  subThread(subPeer); // `subPeer` is connected

    // Start an exchange
    waitForState(PUB_PEER_CREATED);
    pubPeer.notify(prodIndex);
    pubPeer.notify(segId);

    waitForState(DONE);

    // Causes `subPeer()` to return and `pubThread` to terminate
    subPeer.halt();

    subThread.join();
    pubThread.join();
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
