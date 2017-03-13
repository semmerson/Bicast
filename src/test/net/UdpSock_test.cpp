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

#include "error.h"
#include "UdpSock.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <iostream>
#include <thread>

namespace {

void runReceiver(hycast::McastUdpSock sock)
{
    try {
        for (size_t size = sock.getSize(); size; size = sock.getSize()) {
            uint8_t buf[size];
            sock.recv(buf, size);
            for (size_t i = 0; i < size; ++i)
                ASSERT_EQ(size%UINT8_MAX, buf[i]);
        }
    }
    catch (const std::exception& e) {
        hycast::log_what(e, __FILE__, __LINE__,
                "Error running UDP multicast receiver");
    }
}

void runSender(hycast::OutUdpSock sock)
{
    try {
#if 0
        for (size_t size = 1; UINT16_MAX-256; size += 1000) {
            uint8_t buf[size];
            ::memset(buf, size%UINT8_MAX, size);
            sock.send(buf, size);
        }
#endif
        uint8_t buf;
        sock.send(&buf, 0);
    }
    catch (const std::exception& e) {
        hycast::log_what(e, __FILE__, __LINE__,
                "Error running UDP multicast sender");
    }
}

// The fixture for testing class UdpSock.
class UdpSockTest : public ::testing::Test {
protected:
    // Objects declared here can be used by all tests in the test case for UdpSock.
    hycast::InetSockAddr localSockAddr;
    hycast::InetSockAddr remoteSockAddr;
    hycast::InetSockAddr mcastSockAddr;
    std::thread          recvThread;
    std::thread          sendThread;

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
};

// Tests input socket construction
TEST_F(UdpSockTest, ServerConstruction) {
    hycast::InUdpSock sock(localSockAddr);
    /*
     * Can't get std::regex to work correctly due to problems with escapes. This
     * occurs when using either ECMAScript and POSIX BRE grammars.
     */
    if (std::string("InUdpSock(addr=localhost:38800, sock=3)") !=
            sock.to_string()) {
        std::cerr << "sock.to_string()=\"" << sock.to_string() << "\"\n";
        ADD_FAILURE();
    }
}

// Tests output socket construction
TEST_F(UdpSockTest, ClientConstruction) {
    hycast::OutUdpSock sock(remoteSockAddr);
    if (std::string("OutUdpSock(addr=zero.unidata.ucar.edu:38800, "
            "sock=3)") != sock.to_string()) {
        std::cerr << "sock.to_string()=\"" << sock.to_string() << "\"\n";
        ADD_FAILURE();
    }
    char buf[1];
    sock.send(buf, sizeof(buf));
}

// Tests output socket construction to the local host
TEST_F(UdpSockTest, LocalhostConstruction) {
    hycast::OutUdpSock sock(localSockAddr);
    if (std::string("OutUdpSock(addr=localhost:38800, sock=3)") !=
            sock.to_string()) {
        std::cerr << "sock.to_string()=\"" << sock.to_string() << "\"\n";
        ADD_FAILURE();
    }
    char buf[1000];
    sock.send(buf, sizeof(buf));
}

// Tests source-independent multicast socket construction
TEST_F(UdpSockTest, MulticastConstruction) {
    hycast::McastUdpSock sock(mcastSockAddr);
    if (std::string("McastUdpSock(addr=234.128.117.0:38800, sock=3)") !=
            sock.to_string()) {
        std::cerr << "sock.to_string()=\"" << sock.to_string() << "\"\n";
        ADD_FAILURE();
    }
}

// Tests source-specific multicast socket construction
TEST_F(UdpSockTest, SourceMulticastConstruction) {
    hycast::McastUdpSock sock(mcastSockAddr, "localhost");
    if (std::string("McastUdpSock(addr=234.128.117.0:38800, sock=3)") !=
            sock.to_string()) {
        std::cerr << "sock.to_string()=\"" << sock.to_string() << "\"\n";
        ADD_FAILURE();
    }
}

// Tests source-independent multicasting
TEST_F(UdpSockTest, Multicasting) {
    hycast::McastUdpSock recvSock(mcastSockAddr);
    std::thread recvThread{runReceiver, recvSock};
    ::sleep(1);
    hycast::OutUdpSock sendSock(mcastSockAddr);
    std::thread sendThread{runSender, sendSock};
    sendThread.join();
    recvThread.join();
}

// Tests source-specific multicasting
TEST_F(UdpSockTest, SourceMulticasting) {
    hycast::OutUdpSock   sendSock(mcastSockAddr);
    auto                 sourceAddr = sendSock.getLocalAddr().getInetAddr();
    hycast::McastUdpSock recvSock(mcastSockAddr, sourceAddr);
    std::thread recvThread{runReceiver, recvSock};
    ::sleep(1);
    std::thread sendThread{runSender, sendSock};
    sendThread.join();
    recvThread.join();
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
