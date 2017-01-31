/**
 * This file tests the class `UdpSock`
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: UdpSock_test.cpp
 * @author: Steven R. Emmerson
 */

#include "UdpSock.h"

#include <gtest/gtest.h>

namespace {

// The fixture for testing class UdpSock.
class UdpSockTest : public ::testing::Test {
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    UdpSockTest()
        : localSockAddr{"localhost", 38800}
        , remoteSockAddr{"zero.unidata.ucar.edu", 38800}
        // UCAR unicast-based multicast address:
        , mcastSockAddr{"234.128.117.0", 38800}
    {}

    virtual ~UdpSockTest() {
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

    // Objects declared here can be used by all tests in the test case for UdpSock.
    hycast::InetSockAddr localSockAddr;
    hycast::InetSockAddr remoteSockAddr;
    hycast::InetSockAddr mcastSockAddr;
};

// Tests server construction
TEST_F(UdpSockTest, ServerConstruction) {
    hycast::InUdpSock sock(localSockAddr);
    /*
     * Can't get std::regex to work correctly due to problems with escapes. This
     * occurs when using either ECMAScript and POSIX BRE grammars.
     */
    if (std::string("SrvrUdpSock(localAddr=localhost:38800, sock=3)") !=
            sock.to_string()) {
        std::cerr << "sock.to_string()=\"" << sock.to_string() << "\"\n";
        ADD_FAILURE();
    }
}

// Tests client construction
TEST_F(UdpSockTest, ClientConstruction) {
    hycast::InUdpSock sock(localSockAddr);
    if (std::string("ClntUdpSock(localAddr=localhost:0, sock=3)") !=
            sock.to_string()) {
        std::cerr << "sock.to_string()=\"" << sock.to_string() << "\"\n";
        ADD_FAILURE();
    }
}

// Tests multicast construction
TEST_F(UdpSockTest, MulticastConstruction) {
    hycast::McastUdpSock sock(mcastSockAddr);
    if (std::string("McastUdpSock(mcastAddr=234.128.117.0:38800, sock=3)")
            != sock.to_string()) {
        std::cerr << "sock.to_string()=\"" << sock.to_string() << "\"\n";
        ADD_FAILURE();
    }
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
