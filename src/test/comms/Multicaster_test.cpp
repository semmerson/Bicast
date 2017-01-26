/**
 * This file tests the Multicaster class.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Multicaster.cpp
 * @author: Steven R. Emmerson
 */

#include "Multicaster.h"
#include "InetAddr.h"
#include "InetSockAddr.h"

#include <gtest/gtest.h>

namespace {

// The fixture for testing class UdpRcvr.
class MulticasterTest : public ::testing::Test {
protected:
    class MsgRcvr : public hycast::MsgRcvr
    {
        void recvNotice(const hycast::ProdInfo& info, hycast::Peer& peer) {}
        void recvNotice(const hycast::ChunkInfo& info, hycast::Peer& peer) {}
        void recvRequest(const hycast::ProdIndex& index, hycast::Peer& peer) {}
        void recvRequest(const hycast::ChunkInfo& info, hycast::Peer& peer) {}
        void recvData(hycast::LatentChunk chunk, hycast::Peer& peer) {}
    };

    // You can remove any or all of the following functions if its body
    // is empty.

    MulticasterTest()
        : localAddr{"localhost"}
        , localSockAddr{localAddr, 38800}
        , remoteSockAddr{"zero.unidata.ucar.edu", 38800}
        // UCAR unicast-based multicast address:
        , mcastSockAddr{"234.128.117.0", 38800}
        , sock{mcastSockAddr, localAddr}
        , prodInfo{"product", 1, 3, 2}
        , version{0}
    {}

    virtual ~MulticasterTest() {
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
    MsgRcvr              msgRcvr;
    hycast::ProdInfo     prodInfo;
    unsigned             version;
};

// Tests default construction
TEST_F(MulticasterTest, DefaultConstruction) {
    hycast::Multicaster mcaster();
}

// Tests construction
TEST_F(MulticasterTest, Construction) {
    hycast::Multicaster mcaster(sock, 0, &msgRcvr);
}

// Tests object transmission
TEST_F(MulticasterTest, Transmission) {
    hycast::Multicaster mcaster(sock, version, &msgRcvr);
    sock.setMcastLoop(true).setHopLimit(0);
    //char buf[mcaster.getSerialSize(version, prodInfo)];
    //sock.send(buf, msglen);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
