/**
 * This file tests the `Multicast` module.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: Multicast_test.cpp
 * Created On: Oct 15, 2019
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "error.h"
#include "Multicast.h"

#include <condition_variable>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>

namespace {

/// The fixture for testing module `Multicast`
class MulticastTest : public ::testing::Test
{
protected:
    hycast::SockAddr        grpAddr; // Multicast group address
    std::mutex              mutex;
    std::condition_variable cond;
    bool                    ready;

    MulticastTest()
        : grpAddr("232.1.1.1:38800")
        , mutex()
        , cond()
        , ready{false}
    {
    }

    void runSink(
            const hycast::InetAddr& srcAddr,
            const hycast::MemChunk& memChunk)
    {
        // Create multicast receiver
        hycast::McastRcvr mcastRcvr(grpAddr, srcAddr);

        // Notify multicast sender that the receiver is ready
        {
            std::lock_guard<decltype(mutex)> guard{mutex};
            ready = true;
            cond.notify_one();
        }

        // Receive the multicast chunk
        hycast::UdpChunk udpChunk{};
        mcastRcvr.recv(udpChunk);

        // Verify the chunk
        EXPECT_EQ(memChunk.getId(), udpChunk.getId());
        hycast::ChunkSize memSize = memChunk.getSize();
        hycast::ChunkSize udpSize = udpChunk.getSize();
        ASSERT_EQ(memSize, udpSize);
        const char* memData = static_cast<const char*>(memChunk.getData());
        char        udpData[udpSize];
        udpChunk.read(udpData);
        EXPECT_STREQ(memData, udpData);
    }
};

// Tests construction
TEST_F(MulticastTest, Construction)
{
    hycast::McastSndr mcastSndr(grpAddr);
}

// Tests multicasting
TEST_F(MulticastTest, Multicasting)
{
    // Create chunk
    const hycast::ChunkId   id{1};
    const char              memData[] = "Hello, world!";
    const hycast::ChunkSize memSize = sizeof(memData);
    const hycast::MemChunk  memChunk(id, memSize, memData);

    // Create multicast sender
    hycast::McastSndr mcastSndr(grpAddr);

    // Create multicast sink
    hycast::SockAddr  lclAddr = mcastSndr.getLclAddr();
    hycast::InetAddr  srcAddr = lclAddr.getInetAddr();
    std::thread       snkThread(&MulticastTest_Multicasting_Test::runSink, this,
            std::ref(srcAddr), std::ref(memChunk));

    // Wait until multicast sink is ready
    std::unique_lock<decltype(mutex)> lock{mutex};
    while (!ready)
        cond.wait(lock);

    // Multicast memChunk
    mcastSndr.send(memChunk);

    // Join multicast sink
    snkThread.join();
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
