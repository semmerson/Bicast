/**
 * This file tests sockets.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: socket_test.cpp
 * @author: Steven R. Emmerson
 */


#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <thread>

#include <gtest/gtest.h>

namespace {

// The fixture for testing sockets.
class SocketTest : public ::testing::Test {
protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    SocketTest() {
        // You can do set-up work for each test here.
        ipv4SockAddr.sin_family = AF_INET;
        ipv4SockAddr.sin_port = htons(38800);
        //ipv4SockAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // works
        ipv4SockAddr.sin_addr.s_addr = inet_addr("234.128.117.0"); // works

        *reinterpret_cast<struct sockaddr_in*>(&sockAddrStorage) = ipv4SockAddr;
    }

    virtual ~SocketTest() {
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

    void joinMcastGroup(const int sd)
    {
        struct group_req req = {};
        req.gr_group = sockAddrStorage;
        //req.gr_interface = 2; // Ethernet interface. Makes no difference
        req.gr_interface = 0; // Use default multicast interface
        int status = ::setsockopt(sd, IPPROTO_IP, MCAST_JOIN_GROUP, &req,
                sizeof(req));
        EXPECT_EQ(0, status);
    }

    int makeSendSocket()
    {
        int       sd = ::socket(AF_INET, SOCK_DGRAM, 0);
        EXPECT_NE(-1, sd);

        int status = ::connect(sd,
                reinterpret_cast<struct sockaddr*>(&ipv4SockAddr),
                sizeof(ipv4SockAddr)); // Makes no difference
        EXPECT_NE(-1, status);

        unsigned char hopLimit = 1;
        socklen_t     socklen = sizeof(hopLimit);
        status = ::setsockopt(sd, IPPROTO_IP, IP_MULTICAST_TTL, &hopLimit,
                socklen); // Makes no difference
        EXPECT_EQ(0, status);

        return sd;
    }

    int makeRecvSocket()
    {
        int       sd = ::socket(AF_INET, SOCK_DGRAM, 0);
        EXPECT_NE(-1, sd);

        int status = ::bind(sd, reinterpret_cast<struct sockaddr*>(&ipv4SockAddr),
                sizeof(ipv4SockAddr));
        EXPECT_NE(-1, status);

        joinMcastGroup(sd);

        return sd;
    }

    void send(const int sd)
    {
        unsigned short size = 1;
        struct iovec   iov;
        iov.iov_base = &size;
        iov.iov_len = sizeof(size);
        struct msghdr msghdr = {};
        ::memset(&msghdr, 0, sizeof(msghdr)); // Makes no difference
        msghdr.msg_name = &sockAddrStorage;
        msghdr.msg_namelen = sizeof(sockAddrStorage);
        // msghdr.msg_namelen = sizeof(struct sockaddr_in); // Makes no difference
        msghdr.msg_iov = const_cast<struct iovec*>(&iov);
        msghdr.msg_iovlen = 1;
        int status = ::sendmsg(sd, &msghdr, 0); // MSG_EOR makes no difference
        EXPECT_EQ(iov.iov_len, status);
    }

    void recv(const int sd)
    {
        unsigned short size;
        struct iovec   iov;
        iov.iov_base = &size;
        iov.iov_len = sizeof(size);
        struct msghdr msghdr = {};
        msghdr.msg_iov = const_cast<struct iovec*>(&iov);
        msghdr.msg_iovlen = 1;
        msghdr.msg_name = nullptr; // Makes no difference
        ssize_t status = ::recvmsg(sd, &msghdr, 0);
        EXPECT_EQ(iov.iov_len, status);
        EXPECT_EQ(1, size);
    }

    // Objects declared here can be used by all tests in the test case for sockets.
    struct sockaddr_in      ipv4SockAddr = {};
    struct sockaddr_storage sockAddrStorage = {};
};

// Tests multicasting
TEST_F(SocketTest, Multicasting) {
    int sendSock = makeSendSocket();
    int recvSock = makeRecvSocket();
    std::thread sendThread([this,sendSock]{send(sendSock);});
    std::thread recvThread([this,recvSock]{recv(recvSock);});
    recvThread.join();
    sendThread.join();
    ::close(recvSock);
    ::close(sendSock);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
