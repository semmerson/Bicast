/**
 * This file tests class `NoticeQueue`.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: NoticeQueue_test.cpp
 * Created On: May 19, 2021
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "NoticeQueue.h"

#include <P2pMgr.h>
#include "gtest/gtest.h"

namespace {

using namespace hycast;

/// The fixture for testing class `NoticeQueue`
class NoticeQueueTest : public ::testing::Test, public hycast::P2pMgr
{
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    NoticeQueueTest()
    {
        // You can do set-up work for each test here.
    }

    virtual ~NoticeQueueTest()
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
    void offline(Peer peer) override {
    }
    void reassigned(const hycast::ProdIndex  notice,
                    const SockAddr           rmtAddr) override {
    }
    void reassigned(const hycast::DataSegId& notice,
                    const SockAddr           rmtAddr) override {
    }
};

// Tests default construction
TEST_F(NoticeQueueTest, DefaultConstruction)
{
    hycast::NoticeQueue noticeQ{*this};
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
