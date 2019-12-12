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
#include "config.h"
#include "error.h"
#include "SockAddr.h"

#include <atomic>
#include <condition_variable>
#include <gtest/gtest.h>
#include <PeerProto.h>
#include <mutex>
#include <thread>

namespace {

/// The fixture for testing class `PeerProto`
class PeerProtoTest : public ::testing::Test, public hycast::PeerProtoObs
{
protected:
    typedef enum {
        INIT = 0,
        READY = 1,
        PROD_NOTICE_RCVD = 2,
        SEG_NOTICE_RCVD = 4,
        PROD_REQUEST_RCVD = 8,
        SEG_REQUEST_RCVD = 16,
        PROD_INFO_RCVD = 32,
        SEG_RCVD = 64,
        DONE = READY |
               PROD_NOTICE_RCVD |
               SEG_NOTICE_RCVD |
               PROD_REQUEST_RCVD |
               SEG_REQUEST_RCVD |
               PROD_INFO_RCVD |
               SEG_RCVD
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

    PeerProtoTest()
        : state{INIT}
        , mutex{}
        , cond{}
        , srvrAddr{"localhost:38800"}
        /*
         * 3 potential port numbers for the client's 2 temporary servers because
         * the initial client connection could use one
         */
        , portPool(38801, 3)
        , prodIndex{1}
        , prodSize{1000000}
        , segSize{1000}
        , prodInfo{prodIndex, prodSize, "product"}
        , segId(prodIndex, segSize)
        , segInfo(segId, prodSize, segSize)
        , memData{new char[segSize]}
        , memSeg{segInfo, memData}
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
        this->state = static_cast<State>(this->state | state);
        cond.notify_all();
    }

    void waitForState(const State state)
    {
        std::unique_lock<decltype(mutex)> lock{mutex};
        while (this->state != state)
            cond.wait(lock);
    }

public:
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
        const hycast::SegSize size = seg.getInfo().getSegSize();
        ASSERT_EQ(segSize, size);

        char buf[size];
        seg.write(buf);

        EXPECT_EQ(0, ::memcmp(memSeg.getData(), buf, segSize));

        orState(SEG_RCVD);
    }

    void startServer()
    {
        try {
            hycast::TcpSrvrSock srvrSock(srvrAddr);

            // Must be after listening socket creation and before accept
            orState(READY);

            hycast::TcpSock    sock{srvrSock.accept()};
            hycast::PeerProto  srvrProto(sock, portPool);
            srvrProto.set(this);

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
        waitForState(READY);

        // Start client
        hycast::PeerProto clntProto{srvrAddr};
        clntProto.set(this);
        //std::thread       clntThread(&hycast::PeerProto::operator(), clntProto);
        std::thread       clntThread(clntProto);

        try {
            // Client and server are connected

            // Send
            clntProto.notify(prodIndex);
            //::pause();
            clntProto.notify(segId);
            clntProto.request(prodIndex);
            clntProto.request(segId);
            clntProto.send(prodInfo);
            clntProto.send(memSeg);

            waitForState(DONE);

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
        srvrThread.join();
        throw;
    }
    catch (...) {
        LOG_FATAL("Thread cancellation?");
        srvrThread.join();
        throw;
    }
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
