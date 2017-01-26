/**
 * This file the UdpRcvr class.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: UdpRcvr_test.cpp
 * @author: Steven R. Emmerson
 */

#include <comms/McastRcvr.h>
#include "InetAddr.h"
#include "InetSockAddr.h"
#include <gtest/gtest.h>

namespace {

// The fixture for testing class UdpRcvr.
class McastRcvrTest : public ::testing::Test {
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    McastRcvrTest()
        : localAddr{"localhost"}
        , localSockAddr{localAddr, 38800}
        , remoteSockAddr{"zero.unidata.ucar.edu", 38800}
        // UCAR unicast-based multicast address:
        , mcastSockAddr{"234.128.117.0", 38800}
        , sock{mcastSockAddr, localAddr}
    {}

    virtual ~McastRcvrTest() {
        // You can do clean-up work that doesn't throw exceptions here.
    }

    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:

    virtual void SetUp() {
        // Code here will be called immediately after the constructor (right
        // before each test).
    }

    virtual void TearDown() {
        // Code here will be called immediately after each test (right
        // before the destructor).
    }

    // Objects declared here can be used by all tests in the test case for UdpRcvr.
    hycast::InetAddr     localAddr;
    hycast::InetSockAddr localSockAddr;
    hycast::InetSockAddr remoteSockAddr;
    hycast::InetSockAddr mcastSockAddr;
    hycast::McastUdpSock sock;
};

// Tests default construction
TEST_F(McastRcvrTest, DefaultConstruction) {
    hycast::McastRcvr      rcvr();
}

// Tests construction
TEST_F(McastRcvrTest, Construction) {
    hycast::McastRcvr      rcvr(sock, 0);
}

// Tests receiving objects
TEST_F(McastRcvrTest, Receiving) {
    hycast::McastRcvr      rcvr(sock, 0);

}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
