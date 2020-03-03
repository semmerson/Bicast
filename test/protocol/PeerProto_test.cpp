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

#include <error.h>
#include <gtest/gtest.h>
#include <gtest/internal/gtest-internal.h>
#include <PeerProto.h>
#include <SockAddr.h>
#include <condition_variable>
#include <cstring>
#include <exception>
#include <mutex>
#include <thread>

namespace {

/// The fixture for testing class `PeerProto`
class PeerProtoTest : public ::testing::Test, public hycast::PeerProtoObs
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
    hycast::SockAddr        srvrAddr;
    hycast::PortPool        portPool;
    hycast::ProdIndex       prodIndex;
    hycast::ProdSize        prodSize;
    hycast::ProdInfo        prodInfo;
    hycast::SegSize         segSize;
    hycast::SegId           segId;
    hycast::SegInfo         segInfo;
    char*                   memData;
    hycast::MemSeg          memSeg;
    hycast::PeerProto       srvrProto;

    PeerProtoTest()
        : state{INIT}
        , mutex{}
        , cond{}
        , srvrAddr{"localhost:3880"}
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
        , srvrProto{}
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
    void pathToSrc() noexcept
    {
        orState(PATH_TO_SRC_RCVD);
    }

    void noPathToSrc() noexcept
    {
        orState(NO_PATH_TO_SRC_RCVD);
    }

    void acceptNotice(hycast::ProdIndex actual)
    {
        EXPECT_EQ(prodIndex, actual);
        orState(PROD_NOTICE_RCVD);
    }

    void acceptNotice(const hycast::SegId& actual)
    {
        EXPECT_EQ(segId, actual);
        orState(SEG_NOTICE_RCVD);
    }

    void acceptRequest(hycast::ProdIndex actual)
    {
        EXPECT_EQ(prodIndex, actual);
        orState(PROD_REQUEST_RCVD);
    }

    void acceptRequest(const hycast::SegId& actual)
    {
        EXPECT_EQ(segId, actual);
        orState(SEG_REQUEST_RCVD);
    }

    void accept(const hycast::ProdInfo& actual)
    {
        EXPECT_EQ(prodInfo, actual);
        orState(PROD_INFO_RCVD);
    }

    void accept(hycast::TcpSeg& seg)
    {
        const hycast::SegSize size = seg.getSegInfo().getSegSize();
        ASSERT_EQ(segSize, size);

        char buf[size];
        seg.read(buf);

        const auto cmp = ::memcmp(memSeg.data(), buf, segSize);
        EXPECT_EQ(0, cmp);

        orState(SEG_RCVD);
    }

    void startServer()
    {
        try {
            hycast::TcpSrvrSock srvrSock(srvrAddr);

            // Must be after listening socket creation and before accept
            orState(LISTENING);

            hycast::TcpSock    sock{srvrSock.accept()};
            srvrProto = hycast::PeerProto(sock, portPool, *this, true);

            orState(CONNECTED);

            srvrProto();
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
    hycast::PeerProto peerProto();
}

// Tests exchange
TEST_F(PeerProtoTest, Exchange)
{
    // Start server
    std::thread srvrThread{&PeerProtoTest::startServer, this};

    try {
        waitForBit(LISTENING);

        // Start client
        hycast::PeerProto clntProto{srvrAddr, *this};
        std::thread       clntThread(clntProto);

        try {
            waitForBit(CONNECTED);

            // Send
            srvrProto.notify(prodIndex);
            waitForBit(PROD_NOTICE_RCVD);
            clntProto.request(prodIndex);
            waitForBit(PROD_REQUEST_RCVD);
            srvrProto.send(prodInfo);
            waitForBit(PROD_INFO_RCVD);

            srvrProto.notify(segId);
            waitForBit(SEG_NOTICE_RCVD);
            clntProto.request(segId);
            waitForBit(SEG_REQUEST_RCVD);
            srvrProto.send(memSeg);
            waitForBit(SEG_RCVD);

            clntProto.halt();
            clntThread.join();
            srvrThread.join();
        } // `clntThread` allocated
        catch (const std::exception& ex) {
            hycast::log_fatal(ex);
            clntThread.join();
            throw;
        }
        catch (...) {
            LOG_FATAL("Thread cancellation?");
            clntThread.join();
            throw;
        }
    } // `srvrThread` allocated
    catch (const std::exception& ex) {
        hycast::log_fatal(ex);
        if (srvrProto)
            srvrProto.halt();
        srvrThread.join();
        throw;
    }
    catch (...) {
        LOG_FATAL("Thread cancellation?");
        srvrProto.halt();
        srvrThread.join();
        throw;
    }
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
