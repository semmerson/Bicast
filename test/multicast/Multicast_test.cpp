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
    {}

public:
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
        ASSERT_EQ(memChunk.getSize(), udpChunk.getSize());
        char buf[udpChunk.getSize()];
        udpChunk.read(buf);
        EXPECT_STREQ(static_cast<const char*>(memChunk.getData()), buf);
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
    hycast::ChunkId  id{1};
    const char       data[] = "Hello, world!";
    hycast::MemChunk chunk(id, sizeof(data), data);

    // Create multicast sink
    hycast::McastSndr mcastSndr(grpAddr);
    hycast::SockAddr  lclAddr = mcastSndr.getLclAddr();
    hycast::InetAddr  srcAddr = lclAddr.getInetAddr();
    std::thread       snkThread(&MulticastTest::runSink, this,
            std::ref(srcAddr), std::ref(chunk));

    // Wait until multicast sink is ready
    std::unique_lock<decltype(mutex)> lock{mutex};
    while (!ready)
        cond.wait(lock);

    // Multicast chunk
    mcastSndr.send(chunk);

    // Join multicast sink
    snkThread.join();
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
