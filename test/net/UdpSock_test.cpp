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
#include <cstring>
#include <gtest/gtest.h>
#include <iostream>
#include <thread>

namespace {

void runReceiver(hycast::McastUdpSock sock)
{
    try {
        unsigned char   buf[hycast::UdpSock::maxPayload];
        for (size_t expectedSize = 1;
                expectedSize < hycast::UdpSock::maxPayload; expectedSize *= 2) {
            auto const size = sock.recv(buf, sizeof(buf));
            ASSERT_EQ(expectedSize, size);
            for (size_t i = 0; i < size; ++i) {
                unsigned char value = size | 0xff;
                ASSERT_EQ(value, buf[i]);
            }
        }
    }
    catch (const std::exception& e) {
        hycast::log_error(e);
        LOG_ERROR("Error running UDP multicast receiver");
    }
}

void runSender(hycast::OutUdpSock sock)
{
    try {
        for (size_t size = 1; size < hycast::UdpSock::maxPayload; size *= 2) {
            uint8_t buf[size];
            int value = size | 0xff;
            ::memset(buf, value, size);
            sock.send(buf, size);
        }
    }
    catch (const std::exception& e) {
        hycast::log_error(e);
        LOG_ERROR("Error running UDP multicast sender");
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
TEST_F(UdpSockTest, InputConstruction) {
    hycast::InUdpSock sock(localSockAddr);
    /*
     * Can't get std::regex to work correctly due to problems with escapes. This
     * occurs when using either ECMAScript and POSIX BRE grammars.
     */
    if (::strstr(sock.to_string().c_str(),
            "InUdpSock(addr=localhost:38800, sock=") == nullptr) {
        std::cerr << "sock.to_string()=\"" << sock.to_string() << "\"\n";
        ADD_FAILURE();
    }
}

// Tests output socket construction
TEST_F(UdpSockTest, OutputConstruction) {
    hycast::OutUdpSock sock(remoteSockAddr);
    if (::strstr(sock.to_string().c_str(),
            "OutUdpSock(addr=zero.unidata.ucar.edu:38800, sock=") == nullptr) {
        std::cerr << "sock.to_string()=\"" << sock.to_string() << "\"\n";
        ADD_FAILURE();
    }
    char buf[1];
    sock.send(buf, sizeof(buf));
}

// Tests output socket construction to the local host
TEST_F(UdpSockTest, LocalhostOutputConstruction) {
    hycast::OutUdpSock sock(localSockAddr);
    if (::strstr(sock.to_string().c_str(),
            "OutUdpSock(addr=localhost:38800, sock=") == nullptr) {
        std::cerr << "sock.to_string()=\"" << sock.to_string() << "\"\n";
        ADD_FAILURE();
    }
    char buf[1000];
    sock.send(buf, sizeof(buf));
}

// Tests source-independent multicast socket construction
TEST_F(UdpSockTest, AnySourceConstruction) {
    hycast::McastUdpSock sock(mcastSockAddr);
    if (::strstr(sock.to_string().c_str(),
            "McastUdpSock(addr=234.128.117.0:38800, sock=") == nullptr) {
        std::cerr << "sock.to_string()=\"" << sock.to_string() << "\"\n";
        ADD_FAILURE();
    }
}

// Tests source-specific multicast socket construction
TEST_F(UdpSockTest, SourceSpecificConstruction) {
    hycast::McastUdpSock sock(mcastSockAddr, "localhost");
    if (::strstr(sock.to_string().c_str(),
            "McastUdpSock(addr=234.128.117.0:38800, sock=") == nullptr) {
        std::cerr << "sock.to_string()=\"" << sock.to_string() << "\"\n";
        ADD_FAILURE();
    }
}

// Tests source-independent multicasting
TEST_F(UdpSockTest, AnySourceMulticasting) {
    hycast::McastUdpSock recvSock(mcastSockAddr);
    std::thread recvThread{runReceiver, recvSock};
    ::usleep(100000);
    hycast::OutUdpSock sendSock(mcastSockAddr);
    std::thread sendThread{runSender, sendSock};
    sendThread.join();
    recvThread.join();
}

// Tests source-specific multicasting
TEST_F(UdpSockTest, SourceSpecificMulticasting) {
    hycast::OutUdpSock   sendSock(mcastSockAddr);
    auto                 sourceAddr = sendSock.getLocalAddr().getInetAddr();
    hycast::McastUdpSock recvSock(mcastSockAddr, sourceAddr);
    std::thread recvThread{runReceiver, recvSock};
    ::usleep(100000);
    std::thread sendThread{runSender, sendSock};
    sendThread.join();
    recvThread.join();
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
