/**
 * This file tests class `Xprt`.
 *
 *   Copyright 2022 University Corporation for Atmospheric Research
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
 *
 *       File: Xprt_test.cpp
 * Created On: Jan 15, 2022
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "Xprt.h"

#include <gtest/gtest.h>
#include <pthread.h>
#include <random>
#include <thread>

namespace {

using namespace hycast;
using namespace std;

/// The fixture for testing class `Xprt`
class XprtTest : public ::testing::Test
{
protected:
    SockAddr srvrAddr{"localhost:38800"};
    using PduId = enum : uint8_t {
        PDU_ID,
        PDU_BOOL,
        PDU_UINT8,
        PDU_UINT16,
        PDU_UINT32,
        PDU_UINT64,
        PDU_BYTES,
        PDU_STRING8,
        PDU_STRING16,
        PDU_STRING32,
        PDU_STRING64,
        PDU_NUM
    };

    const bool     boolVal   = true;
    const uint8_t  uint8Val  = 1;
    const uint16_t uint16Val = 2;
    const uint32_t uint32Val = 3;
    const uint64_t uint64Val = 4;
    uint8_t        bytesVal[99];
    string         stringVal;
    const int      NUM_SENDS = 10000;

    // You can remove any or all of the following functions if its body
    // is empty.

    XprtTest()
    {
        for (int i = 0; i < sizeof(bytesVal); ++i)
            bytesVal[i] = i;
        stringVal = "This is a test of the transport class";
        // You can do set-up work for each test here.
    }

    virtual ~XprtTest()
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

    void startRcvr(TcpSrvrSock srvrSock) {
        auto sock = srvrSock.accept();
        Xprt xprt{sock};

        for (int i = 0; i < NUM_SENDS; ++i)
        //for (int i = 0; i < PDU_NUM; ++i)
        {
            uint8_t    pduId;
            const bool success = xprt.read(pduId);
            EXPECT_TRUE(success);

            switch (pduId) {
                case PDU_ID: {
                    break;
                }
                case PDU_BOOL: {
                    bool value;
                    ASSERT_TRUE(xprt.read(value));
                    ASSERT_EQ(boolVal, value);
                    break;
                }
                case PDU_UINT8: {
                    uint8_t value;
                    ASSERT_TRUE(xprt.read(value));
                    ASSERT_EQ(uint8Val, value);
                    break;
                }
                case PDU_UINT16: {
                    uint16_t value;
                    ASSERT_TRUE(xprt.read(value));
                    ASSERT_EQ(uint16Val, value);
                    break;
                }
                case PDU_UINT32: {
                    uint32_t value;
                    ASSERT_TRUE(xprt.read(value));
                    ASSERT_EQ(uint32Val, value);
                    break;
                }
                case PDU_UINT64: {
                    uint64_t value;
                    ASSERT_TRUE(xprt.read(value));
                    ASSERT_EQ(uint64Val, value);
                    break;
                }
                case PDU_BYTES: {
                    uint8_t bytes[sizeof(bytesVal)];
                    ASSERT_TRUE(xprt.read(bytes, sizeof(bytes)));
                    ASSERT_EQ(0, ::memcmp(bytesVal, bytes, sizeof(bytes)));
                    break;
                }
                case PDU_STRING8: {
                    std::string string;
                    ASSERT_TRUE(xprt.read<uint8_t>(string));
                    ASSERT_EQ(0, ::strcmp(stringVal.data(), string.data()));
                    break;
                }
                case PDU_STRING16: {
                    std::string string;
                    ASSERT_TRUE(xprt.read<uint16_t>(string));
                    ASSERT_EQ(0, ::strcmp(stringVal.data(), string.data()));
                    break;
                }
                case PDU_STRING32: {
                    std::string string;
                    ASSERT_TRUE(xprt.read<uint32_t>(string));
                    ASSERT_EQ(0, ::strcmp(stringVal.data(), string.data()));
                    break;
                }
                case PDU_STRING64: {
                    std::string string;
                    ASSERT_TRUE(xprt.read<uint64_t>(string));
                    ASSERT_EQ(0, ::strcmp(stringVal.data(), string.data()));
                    break;
                }
                default:
                    abort();
            }
        }
    }

    template<typename TYPE>
    bool send(
            Xprt          xprt,
            const uint8_t pduId,
            const TYPE    datum) {
        auto success = xprt.write(pduId) && xprt.write(datum) && xprt.flush();
        return success;
    }
};

// Tests default construction
TEST_F(XprtTest, DefaultConstruction)
{
    Xprt xprt{};
    EXPECT_FALSE(xprt);
}

TEST_F(XprtTest, Transmission)
{
    TcpSrvrSock srvrSock{srvrAddr};
    thread      rcvrThread(&XprtTest_Transmission_Test::startRcvr, this,
            srvrSock);

    try {
        TcpClntSock                       sock{srvrAddr};
        Xprt                              xprt{sock};
        default_random_engine             generator;
        uniform_int_distribution<uint8_t> distribution(0, PDU_NUM-1);

        for (int i = 0; i < NUM_SENDS; ++i)
        //for (uint8_t pduId = 0; pduId < PDU_NUM; ++pduId)
        {
            bool success;
            auto pduId = distribution(generator);
            switch (pduId) {
                case PDU_ID: {
                    success = xprt.write(pduId) && xprt.flush();
                    break;
                }
                case PDU_BOOL: {
                    success = send<bool>(xprt, pduId, boolVal);
                    break;
                }
                case PDU_UINT8: {
                    success = send<uint8_t>(xprt, pduId, uint8Val);
                    break;
                }
                case PDU_UINT16: {
                    success = send<uint16_t>(xprt, pduId, uint16Val);
                    break;
                }
                case PDU_UINT32: {
                    success = send<uint32_t>(xprt, pduId, uint32Val);
                    break;
                }
                case PDU_UINT64: {
                    success = send<uint64_t>(xprt, pduId, uint64Val);
                    break;
                }
                case PDU_BYTES: {
                    success = xprt.write(pduId) &&
                            xprt.write(bytesVal, sizeof(bytesVal)) &&
                            xprt.flush();
                    break;
                }
                case PDU_STRING8: {
                    success = xprt.write(pduId) &&
                            xprt.write<uint8_t>(stringVal) && xprt.flush();
                    break;
                }
                case PDU_STRING16: {
                    success = xprt.write(pduId) &&
                            xprt.write<uint16_t>(stringVal) && xprt.flush();
                    break;
                }
                case PDU_STRING32: {
                    success = xprt.write(pduId) &&
                            xprt.write<uint32_t>(stringVal) && xprt.flush();
                    break;
                }
                case PDU_STRING64: {
                    success = xprt.write(pduId) &&
                            xprt.write<uint64_t>(stringVal) && xprt.flush();
                    break;
                }
                default:
                    abort();
            }
            EXPECT_TRUE(success);
        }
        rcvrThread.join();
    }
    catch (const std::exception& ex) {
        ::pthread_cancel(rcvrThread.native_handle());
        rcvrThread.join();
        throw;
    }
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
