/**
 * This file tests class `PeerProto`.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: PeerProto_test.cpp
 * Created On: May 28, 2019
 *     Author: Steven R. Emmerson
 */

#include "error.h"
#include "SockAddr.h"
#include "PeerProto.h"

#include <gtest/gtest.h>
#include <condition_variable>
#include <cstring>
#include <exception>
#include <mutex>
#include <thread>

namespace {

/// The fixture for testing class `PeerProto`
class PeerProtoTest
        : public ::testing::Test
        , public hycast::SendPeer
        , public hycast::RecvPeer
{
protected:
    typedef enum {
        INIT = 0,
        LISTENING = 0x1,
        CONNECTED = 0x2,
        PATH_TO_SRC_RCVD = 0x4,
        NO_PATH_TO_SRC_RCVD = 0x8,
        PROD_NOTICE_RCVD = 0x10,
        SEG_NOTICE_RCVD = 0x20,
        PROD_REQUEST_RCVD = 0x40,
        SEG_REQUEST_RCVD = 0x80,
        PROD_INFO_RCVD = 0x100,
        SEG_RCVD = 0x200,
    } State;
    State                   state;
    std::mutex              mutex;
    std::condition_variable cond;
    hycast::SockAddr        pubAddr;
    hycast::PortPool        portPool;
    hycast::ProdIndex       prodIndex;
    hycast::ProdSize        prodSize;
    hycast::ProdInfo        prodInfo;
    hycast::SegSize         segSize;
    hycast::SegId           segId;
    hycast::SegInfo         segInfo;
    char*                   memData;
    hycast::MemSeg          memSeg;
    hycast::PeerProto       pubProto;

    PeerProtoTest()
        : state{INIT}
        , mutex{}
        , cond{}
        , pubAddr{"localhost:3880"}
        /*
         * 3 potential port numbers for the client's 2 temporary servers because
         * the initial client connection could use one
         */
        , portPool(3881, 3)
        , prodIndex{1}
        , prodSize{1000000}
        , segSize{1000}
        , prodInfo{prodIndex, prodSize, "product"}
        , segId(prodIndex, segSize)
        , segInfo(segId, prodSize, segSize)
        , memData{new char[segSize]}
        , memSeg{segInfo, memData}
        , pubProto{}
    {
        ::memset(memData, 0xbd, segSize);
    }

    virtual ~PeerProtoTest()
    {
        delete[] memData;
    }

    void orState(const State state)
    {
        std::lock_guard<decltype(mutex)> guard{mutex};
        this->state = static_cast<State>(this->state | state); // necessary cast
        cond.notify_all();
    }

    void waitForBit(const State state)
    {
        std::unique_lock<decltype(mutex)> lock{mutex};
        while ((this->state & state) == 0)
            cond.wait(lock);
    }

public:
    hycast::SendPeer& asSendPeer() noexcept
    {
        return *this;
    }

    void pathToPub() noexcept
    {
        orState(PATH_TO_SRC_RCVD);
    }

    void noPathToPub() noexcept
    {
        orState(NO_PATH_TO_SRC_RCVD);
    }

    void available(hycast::ProdIndex actual)
    {
        EXPECT_TRUE(prodIndex == actual);
        orState(PROD_NOTICE_RCVD);
    }

    void available(const hycast::SegId& actual)
    {
        EXPECT_EQ(segId, actual);
        orState(SEG_NOTICE_RCVD);
    }

    void sendMe(hycast::ProdIndex actual)
    {
        EXPECT_TRUE(prodIndex == actual);
        orState(PROD_REQUEST_RCVD);
    }

    void sendMe(const hycast::SegId& actual)
    {
        EXPECT_EQ(segId, actual);
        orState(SEG_REQUEST_RCVD);
    }

    void hereIs(const hycast::ProdInfo& actual)
    {
        EXPECT_EQ(prodInfo, actual);
        orState(PROD_INFO_RCVD);
    }

    void hereIs(hycast::TcpSeg& seg)
    {
        const hycast::SegSize size = seg.getSegInfo().getSegSize();
        ASSERT_EQ(segSize, size);

        char buf[size];
        seg.getData(buf);

        const auto cmp = ::memcmp(memSeg.data(), buf, segSize);
        EXPECT_EQ(0, cmp);

        orState(SEG_RCVD);
    }

    void startPub()
    {
        try {
            hycast::TcpSrvrSock srvrSock(pubAddr);

            // Must be after listening socket creation and before accept
            orState(LISTENING);

            hycast::TcpSock    sock{srvrSock.accept()};
            hycast::NodeType   nodeType;
            pubProto = hycast::PeerProto(sock, portPool, *this);

            orState(CONNECTED);

            pubProto();
        }
        catch (const std::exception& ex) {
            hycast::log_error(ex);
            throw;
        }
    }
};

// Tests default construction
TEST_F(PeerProtoTest, DefaultConstruction)
{
    hycast::PeerProto pubProto();
}

// Tests exchange
TEST_F(PeerProtoTest, Exchange)
{
    // Start publisher
    std::thread pubThread{&PeerProtoTest::startPub, this};

    try {
        waitForBit(LISTENING);

        // Start subscriber
        hycast::NodeType  nodeType;
        hycast::PeerProto subProto(pubAddr, nodeType, *this);
        std::thread       subThread(subProto);

        try {
            waitForBit(CONNECTED);

            // Send
            pubProto.notify(prodIndex);
            waitForBit(PROD_NOTICE_RCVD);
            subProto.request(prodIndex);
            waitForBit(PROD_REQUEST_RCVD);
            pubProto.send(prodInfo);
            waitForBit(PROD_INFO_RCVD);

            pubProto.notify(segId);
            waitForBit(SEG_NOTICE_RCVD);
            subProto.request(segId);
            waitForBit(SEG_REQUEST_RCVD);
            pubProto.send(memSeg);
            waitForBit(SEG_RCVD);

            subProto.halt();
            subThread.join();
            pubThread.join();
        } // `clntThread` allocated
        catch (const std::exception& ex) {
            hycast::log_fatal(ex);
            subThread.join();
            throw;
        }
        catch (...) {
            LOG_FATAL("Thread cancellation?");
            subThread.join();
            throw;
        }
    } // `srvrThread` allocated
    catch (const std::exception& ex) {
        hycast::log_fatal(ex);
        if (pubProto)
            pubProto.halt();
        pubThread.join();
        throw;
    }
    catch (...) {
        LOG_FATAL("Thread cancellation?");
        pubProto.halt();
        pubThread.join();
        throw;
    }
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
