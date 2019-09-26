/**
 * This file tests class `Wire`.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: Wire_test.cpp
 * Created On: May 21, 2019
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "Wire.h"

#include <condition_variable>
#include <gtest/gtest.h>
#include <cstdio>
#include <mutex>
#include <thread>

namespace {

/// The fixture for testing class `Wire`
class WireTest : public ::testing::Test
{
protected:
    std::mutex              mutex;
    std::condition_variable cond;
    bool                    srvrReady;

    // You can remove any or all of the following functions if its body
    // is empty.

    WireTest()
        : mutex{}
        , cond{}
        , srvrReady{false}
    {
        // You can do set-up work for each test here.
    }

    virtual ~WireTest()
    {
        // You can do clean-up work that doesn't throw exceptions here.
    }

    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:

    virtual void SetUp()
    {
        // Code here will be called immediately after the constructor (right
        // before each test).
    }

    virtual void TearDown()
    {
        // Code here will be called immediately after each test (right
        // before the destructor).
    }

    // Objects declared here can be used by all tests in the test case for Error.

public:
    void runServer(hycast::SrvrSock& srvrSock)
    {
        {
            std::lock_guard<decltype(mutex)> lock{mutex};
            srvrReady = true;
            cond.notify_one();
        }

        hycast::Socket sock{srvrSock.accept()};
        hycast::Wire   wire(sock);

        for (;;) {
            uint8_t bytes[1500];

            size_t nbytes = wire.deserialize(bytes, sizeof(bytes));
            wire.serialize(bytes, nbytes);
            wire.flush();
        }
    }
};

// Tests default construction
TEST_F(WireTest, DefaultConstruction)
{
    hycast::Wire wire();
}

// Tests scalar serialization
TEST_F(WireTest, ScalarSerialization)
{
    hycast::SockAddr srvrAddr("localhost:38800");
    hycast::SrvrSock srvrSock(srvrAddr);
    std::thread      srvrThread([this,&srvrSock](){runServer(srvrSock);});

    {
        /*
         * The following is necessary because `ClntSock` constructor throws if
         * `::connect()` is called before `::listen()`
         */
        std::unique_lock<decltype(mutex)> lock{mutex};
        while (!srvrReady)
            cond.wait(lock);
    }

    hycast::ClntSock clntSock(srvrAddr);
    hycast::Wire     wire(clntSock);

    ::fprintf(stderr, "ScalarSerialization(): uint8_t\n");
    uint8_t          writeint8 = 0x01;
    wire.serialize(writeint8);
    wire.flush();
    uint8_t readint8 = 0;
    wire.deserialize(readint8);
    EXPECT_EQ(writeint8, readint8);

    ::fprintf(stderr, "ScalarSerialization(): uint16_t\n");
    uint16_t          writeint16 = 0x0102;
    wire.serialize(writeint16);
    wire.flush();
    uint16_t readint16 = 0;
    wire.deserialize(readint16);
    EXPECT_EQ(writeint16, readint16);

    ::fprintf(stderr, "ScalarSerialization(): uint32_t\n");
    uint32_t          writeint32 = 0x01020304;
    wire.serialize(writeint32);
    wire.flush();
    uint32_t readint32 = 0;
    wire.deserialize(readint32);
    EXPECT_EQ(writeint32, readint32);

    ::fprintf(stderr, "ScalarSerialization(): uint64_t\n");
    uint64_t          writeint64 = 0x0102030405060708;
    wire.serialize(writeint64);
    wire.flush();
    uint64_t readint64 = 0;
    wire.deserialize(readint64);
    EXPECT_EQ(writeint64, readint64);

    ::pthread_cancel(srvrThread.native_handle());
    srvrThread.join();
}

// Tests vector serialization
TEST_F(WireTest, VectorSerialization)
{
    hycast::SockAddr srvrAddr("localhost:38800");
    hycast::SrvrSock srvrSock(srvrAddr);
    std::thread      srvrThread([this,&srvrSock](){runServer(srvrSock);});

    {
        // Necessary because `ClntSock` constructor throws if `connect()` fails
        std::unique_lock<decltype(mutex)> lock{mutex};
        while (!srvrReady)
            cond.wait(lock);
    }

    hycast::ClntSock clntSock(srvrAddr);
    hycast::Wire     wire(clntSock);
    uint16_t          writeInt16 = 0x0102;
    uint32_t          writeInt32 = 0x01020304;
    uint64_t          writeInt64 = 0x0102030405060708;
    uint16_t          readInt16 = 0;
    uint32_t          readInt32 = 0;
    uint64_t          readInt64 = 0;

    wire.serialize(writeInt16);
    wire.serialize(writeInt32);
    wire.serialize(writeInt64);
    wire.flush();

    wire.deserialize(readInt16);
    wire.deserialize(readInt32);
    wire.deserialize(readInt64);

    EXPECT_EQ(writeInt16, readInt16);
    EXPECT_EQ(writeInt32, readInt32);
    EXPECT_EQ(writeInt64, readInt64);

    ::pthread_cancel(srvrThread.native_handle());
    srvrThread.join();
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
