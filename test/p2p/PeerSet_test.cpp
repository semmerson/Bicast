/**
 * This file tests class `PeerSet`.
 *
 *       File: PeetSet.cpp
 * Created On: June 11, 2019
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

#include "PeerFactory.h"
#include "PeerSet.h"
#include "SockAddr.h"

#include <atomic>
#include <condition_variable>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>

namespace {

/// The fixture for testing class `PeerSet`
class PeerSetTest
        : public ::testing::Test
        , public hycast::PeerSetMgr
        , public hycast::XcvrPeerMgr
{
protected:
    friend class Subscriber;

    typedef enum {
        INIT = 0,
        PUB_PEER_ACTIVE = 0x1,
        PROD_NOTICE_RCVD = 0x2,
        SEG_NOTICE_RCVD = 0x4,
        PROD_REQUEST_RCVD = 0x8,
        SEG_REQUEST_RCVD = 0x10,
        PROD_INFO_RCVD = 0x20,
        SEG_RCVD = 0x40,
        CLNT_PEER_STOPPED = 0x80,
        SRVR_PEER_STOPPED = 0x100,
        EXCHANGE_COMPLETE = PUB_PEER_ACTIVE |
               PROD_NOTICE_RCVD |
               SEG_NOTICE_RCVD |
               PROD_REQUEST_RCVD |
               SEG_REQUEST_RCVD |
               PROD_INFO_RCVD |
               SEG_RCVD,
        DONE = EXCHANGE_COMPLETE |
               CLNT_PEER_STOPPED |
               SRVR_PEER_STOPPED
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
    hycast::PubPeerFactory  pubFactory;
    hycast::SubPeerFactory  subFactory;
    hycast::Peer            pubPeer;
    hycast::Peer            subPeer;

    // You can remove any or all of the following functions if its body
    // is empty.

    PeerSetTest()
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
        , pubFactory(pubAddr, 1, *this)
        , subFactory(subAddr, 1, *this)
        , pubPeer()
        , subPeer()
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

    // Subscriber-side
    bool shouldRequest(
            const hycast::SockAddr& rmtAddr,
            const hycast::ProdIndex actual)
    {
        EXPECT_TRUE(prodIndex == actual);
        orState(PROD_NOTICE_RCVD);

        return true;
    }

    // Subscriber-side
    bool shouldRequest(
            const hycast::SockAddr& rmtAddr,
            const hycast::SegId&    actual)
    {
        EXPECT_EQ(segId, actual);
        orState(SEG_NOTICE_RCVD);

        return true;
    }

    // Publisher-side
    hycast::ProdInfo getProdInfo(
            const hycast::SockAddr& remote,
            const hycast::ProdIndex actual)
    {
        EXPECT_TRUE(prodIndex == actual);
        orState(PROD_REQUEST_RCVD);
        return prodInfo;
    }

    // Publisher-side
    hycast::MemSeg getMemSeg(
            const hycast::SockAddr& remote,
            const hycast::SegId&    actual)
    {
        EXPECT_EQ(segId, actual);
        orState(SEG_REQUEST_RCVD);
        return memSeg;
    }

    // Subscriber-side
    bool hereIs(
            const hycast::SockAddr& rmtAddr,
            const hycast::ProdInfo& actual)
    {
        EXPECT_EQ(prodInfo, actual);
        orState(PROD_INFO_RCVD);

        return true;
    }

    // Subscriber-side
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

    void stopped(hycast::Peer peer)
    {
        EXPECT_TRUE(peer == pubPeer || peer == subPeer);

        if (peer == pubPeer) {
            orState(SRVR_PEER_STOPPED);
        }
        else if (peer == subPeer) {
            orState(CLNT_PEER_STOPPED);
        }
    }

    void runPub(hycast::PeerSet& peerSet)
    {
        // Calls listen()
        pubPeer = pubFactory.accept();
        EXPECT_EQ(0, peerSet.size());
        peerSet.activate(pubPeer); // Executes peer on new thread
        EXPECT_EQ(1, peerSet.size());
        orState(PUB_PEER_ACTIVE);
    }
};

// Tests default construction
TEST_F(PeerSetTest, DefaultConstruction)
{
    hycast::PeerSet{};
}

// Tests complete exchange (notice, request, delivery)
TEST_F(PeerSetTest, Exchange)
{
    // Start publisher
    hycast::PeerSet pubPeerSet{*this};
    std::thread     pubThread(&PeerSetTest::runPub, this, std::ref(pubPeerSet));

    // Start subscriber
    hycast::PeerSet subPeerSet{*this};
    subPeer = subFactory.connect(pubAddr,
            hycast::NodeType::NO_PATH_TO_PUBLISHER);
    EXPECT_EQ(0, subPeerSet.size());
    subPeerSet.activate(subPeer); // Executes `subPeer` on new thread
    EXPECT_EQ(1, subPeerSet.size());

    // Start an exchange
    waitForState(PUB_PEER_ACTIVE);
    pubPeerSet.notify(prodIndex);
    pubPeerSet.notify(segId);

    // Wait for the exchange to complete
    waitForState(EXCHANGE_COMPLETE);

    // Terminate subscribing peer. Causes publishing-peer to terminate.
    subPeer.halt();

    // Wait for the peers to be removed from their peer-sets
    waitForState(DONE);

    pubThread.join();
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
