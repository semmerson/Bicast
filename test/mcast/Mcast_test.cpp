/**
 * This file tests class `Mcast`.
 *
 *    Copyright 2022 University Corporation for Atmospheric Research
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
 *       File: mcast_test.cpp
 * Created On: Mar 10, 2022
 *     Author: Steven R. Emmerson
 */
#include "config.h"
#include "mcast.h"
#include "Node.h"

#include <cstring>
#include <gtest/gtest.h>
#include <mutex>

using namespace hycast;

namespace {

/// The fixture for testing class `Mcast`
class McastTest : public ::testing::Test, public SubNode
{
protected:
    using State = enum {
        INIT = 0,
        PROD_INFO_RCVD        =  0x1,
        DATA_SEG_RCVD         =  0x2,
    };
    Mutex      mutex;
    Cond       cond;
    unsigned   state;
    SockAddr   ssmAddr;
    InetAddr   srcAddr;
    InetAddr   subIface;
    ProdId  prodIndex;
    String     prodName;
    ProdSize   prodSize;
    ProdInfo   prodInfo;
    DataSegId  segId;
    char       memData[1000];
    DataSeg    dataSeg;

    // You can remove any or all of the following functions if its body
    // is empty.

    McastTest()
        : mutex()
        , cond()
        , state{INIT}
        , ssmAddr{"232.1.1.1:38800"}
        , srcAddr{"127.0.0.1"}
        , subIface{srcAddr}
        , prodIndex{1}
        , prodName{"product"}
        , prodSize{100000}
        , prodInfo(prodIndex, prodName, prodSize)
        , segId(prodIndex, 0)
        , memData()
        , dataSeg()
    {
        DataSeg::setMaxSegSize(sizeof(memData));
        ::memset(memData, 0xbd, sizeof(memData));

        dataSeg = DataSeg(segId, prodSize, memData);
    }

    virtual ~McastTest()
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

    void setState(const State state) {
        std::lock_guard<decltype(mutex)> lock{mutex};
        this->state = state;
        cond.notify_one();
    }

    void orState(const State state)
    {
        std::lock_guard<decltype(mutex)> guard{mutex};
        this->state = static_cast<State>(this->state | state);
        cond.notify_all();
    }

    void waitForState(const unsigned nextState)
    {
        std::unique_lock<decltype(mutex)> lock{mutex};
        cond.wait(lock, [&,nextState]{return state == nextState;});
    }

    void start() {};
    void stop() {};
    void run() {};
    void halt() {};
    void waitForPeer() {}

    void operator()() {
    }

    SockAddr getP2pSrvrAddr() const override {
        return SockAddr{};
    }

    bool shouldRequest(const ProdId index) {
        return true;
    }

    bool shouldRequest(const DataSegId segId) {
        return true;
    }

    ProdInfo recvRequest(const ProdId request) {
        static ProdInfo prodInfo{};

        return prodInfo;
    }

    DataSeg recvRequest(const DataSegId dataSegId) {
        static DataSeg dataSeg{};

        return dataSeg;
    }

    void recvMcastData(const ProdInfo actual) {
        EXPECT_EQ(prodInfo, actual);
        if (prodInfo == actual)
            orState(PROD_INFO_RCVD);
    }

    void recvMcastData(const DataSeg actual) {
        EXPECT_EQ(dataSeg, actual);
        if (dataSeg == actual)
            orState(DATA_SEG_RCVD);
    }
};

// Tests construction
TEST_F(McastTest, Construction)
{
    auto sub = McastSub::create(ssmAddr, srcAddr, subIface, *this);
    auto pub = McastPub::create(ssmAddr, srcAddr);
}

// Tests multicasting product information
TEST_F(McastTest, McastProdInfo)
{
    auto   sub = McastSub::create(ssmAddr, srcAddr, subIface, *this);
    Thread thread{&McastSub::run, sub.get()};
    auto pub = McastPub::create(ssmAddr, srcAddr);
    pub->multicast(prodInfo);
    waitForState(PROD_INFO_RCVD);
    ::pthread_cancel(thread.native_handle());
    thread.join();
}

// Tests multicasting a data segment
TEST_F(McastTest, McastDataSeg)
{
    auto sub = McastSub::create(ssmAddr, srcAddr, subIface, *this);
    Thread thread{&McastSub::run, sub.get()};
    auto pub = McastPub::create(ssmAddr, srcAddr);
    pub->multicast(dataSeg);
    waitForState(DATA_SEG_RCVD);
    ::pthread_cancel(thread.native_handle());
    thread.join();
}

// Tests multicasting a product
TEST_F(McastTest, McastProduct)
{
    auto sub = McastSub::create(ssmAddr, srcAddr, subIface, *this);
    Thread thread{&McastSub::run, sub.get()};
    auto pub = McastPub::create(ssmAddr, srcAddr);
    pub->multicast(prodInfo);
    pub->multicast(dataSeg);
    waitForState(PROD_INFO_RCVD | DATA_SEG_RCVD);
    ::pthread_cancel(thread.native_handle());
    thread.join();
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  log_setLevel(LogLevel::DEBUG);
  return RUN_ALL_TESTS();
}
