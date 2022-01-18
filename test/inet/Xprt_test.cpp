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
    enum {
        PDU_ID,
        PDU_BOOL,
        PDU_UINT8,
        PDU_UINT16,
        PDU_UINT32,
        PDU_UINT64,
        PDU_BYTES
    } PduId;

    const bool     boolVal   = true;
    const uint8_t  uint8Val  = 1;
    const uint16_t uint16Val = 2;
    const uint32_t uint32Val = 3;
    const uint64_t uint64Val = 4;
    const uint8_t  bytesVal[99];
    const string   stringVal;
    const int      NUM_SENDS = 1000;

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

    bool processPdu(
            const Xprt::PduId pduId,
            Xprt              xprt) {
        bool success = false;
        switch (pduId) {
            case PDU_ID: {
                success = true;
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
                uint8_t bytes;
                ASSERT_TRUE(xprt.read(bytes, ));
                ASSERT_EQ(uint16Val, bytes);
                break;
            }
            default:
                abort();
        }
        EXPECT_TRUE(success);
    }

    void startRcvr() {
        TcpSrvrSock srvrSock{srvrAddr};
        auto        sock = srvrSock.accept();
        Xprt        xprt{sock};
        auto        dispatch = [](Xprt::PduId pduId, Xprt xprt) {
                return processPdu(pduId, xprt);};

        for (int i; i < NUM_SENDS; ++i) {
            const bool success = xprt.recv(dispatch);
            EXPECT_TRUE(success);
        }
    }
};

// Tests default construction
TEST_F(XprtTest, DefaultConstruction)
{
    Xprt xprt();
    EXPECT_FALSE(xprt);
}

TEST_F(XprtTest, Transmission)
{
    thread                        rcvrThread(&startRcvr, this);
    TcpClntSock                   sock{srvrAddr};
    Xprt                          xprt{sock};
    default_random_engine         generator;
    uniform_int_distribution<int> distribution(0,6);
    int                           nextPduId = distribution(generator);

    for (int i; i < NUM_SENDS; ++i) {
        auto pduId = nextPduId();
        bool success;
        switch (pduId) {
            case PDU_ID: {
                success = xprt.send(pduId);
                break;
            }
            case PDU_BOOL: {
                success = xprt.send(pduId, boolVal);
                break;
            }
            case PDU_UINT8: {
                success = xprt.send(pduId, uint8Val);
                break;
            }
            case PDU_UINT16: {
                success = xprt.send(pduId, uint16Val);
                break;
            }
            case PDU_UINT32: {
                success = xprt.send(pduId, uint32Val);
                break;
            }
            case PDU_UINT64: {
                success = xprt.send(pduId, uint64Val);
                break;
            }
            case PDU_BYTES: {
                success = xprt.send(pduId, bytesVal, sizeof(bytesVal));
                break;
            }
            default:
                abort();
        }
        EXPECT_TRUE(success);
    }

    rcvrThread.join();
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
