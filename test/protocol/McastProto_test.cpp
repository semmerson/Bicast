/**
 * This file tests the `McastProto` module.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: McastProto_test.cpp
 * Created On: Oct 15, 2019
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "error.h"
#include "McastProto.h"

#include <condition_variable>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>

namespace {

/// The fixture for testing module `McastProto`
class McastProtoTest : public ::testing::Test, public hycast::McastRcvrObs
{
protected:
    hycast::SockAddr        grpAddr; // Multicast group address
    std::mutex              mutex;
    std::condition_variable cond;
    bool                    ready;
    hycast::ProdIndex          prodId;
    hycast::ProdSize        prodSize;
    hycast::ProdInfo        prodInfo;
    hycast::SegSize         segSize;
    hycast::SegId           segId;
    hycast::SegInfo         segInfo;
    char                    memData[1000];
    hycast::MemSeg          memSeg;
    bool                    prodInfoRcvd;
    bool                    segRcvd;

public:
    McastProtoTest()
        : grpAddr("232.1.1.1:3880")
        , mutex()
        , cond()
        , ready{false}
        , prodId{1}
        , prodSize{1000000}
        , segSize{sizeof(memData)}
        , prodInfo{prodId, prodSize, "product"}
        , segId(prodId, segSize)
        , segInfo(segId, prodSize, segSize)
        , memData{}
        , memSeg{segInfo, memData}
        , prodInfoRcvd{false}
        , segRcvd{false}
    {
        ::memset(memData, 0xbd, segSize);
    }

    void hereIs(const hycast::ProdInfo& actual)
    {
        ASSERT_EQ(prodInfo, actual);
        std::lock_guard<decltype(mutex)> guard{mutex};
        prodInfoRcvd = true;
    }

    void hereIs(hycast::UdpSeg& seg)
    {
        const hycast::SegSize size = seg.getInfo().getSegSize();
        ASSERT_EQ(segSize, size);

        char buf[size];
        seg.read(buf);

        ASSERT_EQ(0, ::memcmp(memSeg.getData(), buf, segSize));

        std::lock_guard<decltype(mutex)> guard{mutex};
        segRcvd = true;
        cond.notify_one();
    }

    void runRcvr(hycast::McastRcvr& rcvr)
    {
        // Notify the multicast sender that the receiver is ready
        {
            std::lock_guard<decltype(mutex)> guard{mutex};
            ready = true;
            cond.notify_one();
        }

        //  Receive the multicast
        rcvr(); // Returns on `rcvr.halt()`
    }
};

// Tests multicasting
TEST_F(McastProtoTest, Multicasting)
{
    // Create multicast sender
    hycast::UdpSock   sndSock{grpAddr};
    hycast::McastSndr mcastSndr{sndSock};

    // Create multicast receiver
    hycast::SockAddr  lclAddr = sndSock.getLclAddr();
    hycast::InetAddr  srcAddr = lclAddr.getInetAddr();
    hycast::UdpSock   rcvSock{grpAddr, srcAddr};
    hycast::McastRcvr mcastRcvr{rcvSock, *this};
    std::thread       rcvrThread(&McastProtoTest::runRcvr, this,
            std::ref(mcastRcvr));

    try {
        // Wait until multicast receiver is ready
        {
            std::unique_lock<decltype(mutex)> lock{mutex};
            while (!ready)
                cond.wait(lock);
        }

        // Multicast
        mcastSndr.multicast(prodInfo);
        mcastSndr.multicast(memSeg);

        // Wait until multicast has been received
        {
            std::unique_lock<decltype(mutex)> lock{mutex};
            while (!prodInfoRcvd && !segRcvd)
                cond.wait(lock);
        }

        // Join multicast sink
        mcastRcvr.halt();
        rcvrThread.join();
    }
    catch (const std::exception& ex) {
        hycast::log_fatal(ex);
        mcastRcvr.halt();
        rcvrThread.join();
    }
    catch (...) {
        LOG_FATAL("Thread cancellation?");
        rcvrThread.join();
        throw;
    }
}

}  // namespace

static void myTerminate()
{
    LOG_FATAL("terminate() called %s an active exception",
            std::current_exception() ? "with" : "without");
    abort();
}

int main(int argc, char **argv) {
    hycast::log_setName(::basename(argv[0]));
    hycast::log_setLevel(hycast::LOG_LEVEL_DEBUG);

    std::set_terminate(&myTerminate);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
