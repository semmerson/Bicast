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
    hycast::ProdIndex       prodIndex;
    hycast::ProdSize        prodSize;
    hycast::ProdInfo        prodInfo;
    hycast::SegSize         segSize;
    hycast::SegId           segId;
    hycast::SegInfo         segInfo;
    unsigned char           memData[1000];
    hycast::MemSeg          memSeg;
    bool                    prodInfoRcvd;
    bool                    segRcvd;

public:
    McastProtoTest()
        : grpAddr("232.1.1.1:3880")
        , mutex()
        , cond()
        , ready{false}
        , prodIndex{1}
        , prodSize{1000000}
        , segSize{sizeof(memData)}
        , prodInfo{prodIndex, prodSize, "product"}
        , segId(prodIndex, 0)
        , segInfo(segId, prodSize, segSize)
        , memData{}
        , memSeg{segInfo, memData}
        , prodInfoRcvd{false}
        , segRcvd{false}
    {
        for (int i = 0; i < sizeof(memData); ++i)
            memData[i] = static_cast<unsigned char>(i);
    }

    bool hereIsMcast(const hycast::ProdInfo& actual)
    {
        LOG_DEBUG("prodInfo: %s", prodInfo.to_string().data());
        LOG_DEBUG("actual: %s", actual.to_string().data());
        EXPECT_EQ(prodInfo, actual);
        std::lock_guard<decltype(mutex)> guard{mutex};
        prodInfoRcvd = true;
        return true;
    }

    bool hereIs(hycast::UdpSeg& seg)
    {
        const hycast::SegSize size = seg.getSegInfo().getSegSize();
        EXPECT_EQ(segSize, size);

        char buf[size];
        seg.getData(buf);

        EXPECT_EQ(0, ::memcmp(memSeg.data(), buf, segSize));

        std::lock_guard<decltype(mutex)> guard{mutex};
        segRcvd = true;
        cond.notify_one();

        return true;
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
    hycast::InetAddr      srcAddr = sndSock.getLclAddr().getInetAddr();
    hycast::SrcMcastAddrs mcastAddrs = {.grpAddr=grpAddr, .srcAddr=srcAddr};
    hycast::McastRcvr     mcastRcvr(mcastAddrs, *this);
    std::thread           rcvrThread(&McastProtoTest::runRcvr, this,
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
        ::usleep(5000); // To ensure no packet loss. 2000 failed eventually
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
    auto exPtr = std::current_exception();
    if (exPtr) {
        try {
            std::rethrow_exception(exPtr);
        }
        catch (const std::exception& ex) {
            hycast::log_fatal(ex);
        }
    }
    else {
        LOG_FATAL("terminate() called without active exception");
    }


    abort();
}

int main(int argc, char **argv) {
    hycast::log_setName(::basename(argv[0]));
    hycast::log_setLevel(hycast::LOG_LEVEL_DEBUG);

    std::set_terminate(&myTerminate);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
