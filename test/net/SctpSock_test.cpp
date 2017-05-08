/**
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYRIGHT in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Socket_test.cpp
 * @author: Steven R. Emmerson
 *
 * This file tests class `Socket`.
 */

#include "ClntSctpSock.h"
#include "InetSockAddr.h"
#include "SctpSock.h"
#include "SrvrSctpSock.h"

#include <arpa/inet.h>
#include <atomic>
#include <cstring>
#include <gtest/gtest.h>
#include <fcntl.h>
#include <future>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <thread>
#include <unistd.h>

namespace {

void runServer(hycast::SrvrSctpSock serverSock)
{
    hycast::SctpSock connSock(serverSock.accept());
    for (;;) {
        uint32_t size = connSock.getSize();
        if (size == 0)
        	break;
        unsigned streamId = connSock.getStreamId();
        char buf[size];
        connSock.recv(buf, size);
        connSock.send(streamId, buf, size);
    }
}

void runClient(hycast::ClntSctpSock sock)
{
    // Test Socket::send() and Socket::recv()
    for (int i = 0; i < 100; ++i) {
        unsigned outStream = i % sock.getNumStreams();
        uint8_t outBuf[1+i];
        (void)memset(outBuf, 0xbd, sizeof(outBuf));
        sock.send(outStream, outBuf, sizeof(outBuf));
        uint32_t size = sock.getSize();
        EXPECT_EQ(sizeof(outBuf), size);
        unsigned inStream = sock.getStreamId();
        EXPECT_EQ(inStream, outStream);
        char inBuf[size];
        sock.recv(inBuf, size);
        EXPECT_TRUE(memcmp(inBuf, outBuf, size) == 0);
    }

    // Test Socket::sendv() and Socket::recvv()
    uint8_t outBuf[100];
    for (unsigned i = 0; i < sizeof(outBuf); ++i)
        outBuf[i] = i;
    struct iovec iov[5];
    for (int i = 0; i < 5; ++i) {
        iov[i].iov_base = outBuf + i*20;
        iov[i].iov_len = 20;
    }
    sock.sendv(0, iov, sizeof(iov)/sizeof(iov[0]));
    uint32_t size = sock.getSize();
    EXPECT_EQ(sizeof(outBuf), size);
    unsigned inStream = sock.getStreamId();
    EXPECT_EQ(0, inStream);
    char inBuf[size];
    for (int i = 0; i < 5; ++i) {
        iov[i].iov_base = inBuf + i*20;
        iov[i].iov_len = 20;
    }
    sock.recvv(iov, sizeof(iov)/sizeof(iov[0]), 0);
    for (unsigned i = 0; i < sizeof(outBuf); ++i)
        EXPECT_EQ(outBuf[i], inBuf[i]);

    sock.close();
}

// The fixture for testing class Socket.
class SocketTest : public ::testing::Test {
protected:
    SocketTest() {
		sock1 = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
		sock2 = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
    }

    virtual ~SocketTest() {
        close(sock1);
        close(sock2);
    }

    // Objects declared here can be used by all tests in the test case for Socket.
    const in_port_t      MY_PORT_NUM = 38800;
	const uint16_t       numStreams = 5;
	const std::string    INET_ADDR{"192.168.132.131"};
	hycast::InetSockAddr srvrAddr{INET_ADDR, MY_PORT_NUM};
    int                  sock1, sock2;
    std::thread          recvThread;
    std::thread          sendThread;
    hycast::SrvrSctpSock srvrSock{srvrAddr, numStreams};
    hycast::ClntSctpSock clntSock{srvrAddr, numStreams};

    bool is_open(int sock)
    {
        struct stat statbuf;
        return fstat(sock, &statbuf) == 0;
    }

    void startServer()
    {
        // Work done here so that socket is being listened to when client starts
        recvThread = std::thread(runServer, srvrSock);
    }

    void startClient()
    {
        sendThread = std::thread(runClient, clntSock);
    }

    void stopClient()
    {
        sendThread.join();
    }

    void stopServer()
    {
        recvThread.join();
    }
};

// Tests invalid argument
TEST_F(SocketTest, InvalidArgument) {
    EXPECT_THROW(hycast::SctpSock s(-1), std::invalid_argument);
}

// Tests destruction
TEST_F(SocketTest, CloseOnDestruction) {
    hycast::SctpSock s(sock1);
    EXPECT_EQ(true, is_open(sock1));
    s.~SctpSock();
    EXPECT_EQ(false, is_open(sock1));
}

// Tests copy-construction
TEST_F(SocketTest, CopyConstruction) {
    hycast::SctpSock s1(sock1);
    EXPECT_EQ(true, is_open(sock1));
    hycast::SctpSock s2(s1);
    s1.~SctpSock();
    EXPECT_EQ(true, is_open(sock1));
    s2.~SctpSock();
    EXPECT_EQ(false, is_open(sock1));
}

// Tests move-assignment
TEST_F(SocketTest, MoveAssignment) {
    hycast::SctpSock s1 = hycast::SctpSock(sock1);
    EXPECT_EQ(true, is_open(sock1));
    s1.~SctpSock();
    EXPECT_EQ(false, is_open(sock1));
}

// Tests copy-assignment to empty instance
TEST_F(SocketTest, CopyAssignmentToEmpty) {
    hycast::SctpSock s1(sock1);
    EXPECT_EQ(true, is_open(sock1));
    hycast::SctpSock s2;
    s2 = s1;
    EXPECT_EQ(true, is_open(sock1));
    s1.~SctpSock();
    EXPECT_EQ(true, is_open(sock1));
    s2.~SctpSock();
    EXPECT_EQ(false, is_open(sock1));
}

// Tests copy-assignment to non-empty instance
TEST_F(SocketTest, CopyAssignmentToNonEmpty) {
    hycast::SctpSock s1(sock1);
    EXPECT_EQ(true, is_open(sock1));
    hycast::SctpSock s2(sock2);
    EXPECT_EQ(true, is_open(sock2));
    s2 = s1;
    EXPECT_EQ(true, is_open(sock1));
    EXPECT_EQ(false, is_open(sock2));
    s1.~SctpSock();
    EXPECT_EQ(true, is_open(sock1));
    s2.~SctpSock();
    EXPECT_EQ(false, is_open(sock1));
}

// Tests copy-assignment to self
TEST_F(SocketTest, CopyAssignmentToSelf) {
    hycast::SctpSock s1(sock1);
    EXPECT_EQ(true, is_open(sock1));
    s1 = s1;
    EXPECT_EQ(true, is_open(sock1));
}

// Tests equality operator
TEST_F(SocketTest, EqualityOperator) {
    hycast::SctpSock s1(sock1);
    EXPECT_EQ(true, s1 == s1);
    hycast::SctpSock s2(sock2);
    EXPECT_EQ(false, s2 == s1);
}

// Tests to_string()
TEST_F(SocketTest, ToString) {
    hycast::SctpSock s1(sock1);
    EXPECT_STREQ((std::string("SocketImpl{sock=") + std::to_string(sock1) + "}").data(),
            s1.to_string().data());
}

// Tests whether a read() on a socket returns when the remote end is closed.
TEST_F(SocketTest, RemoteCloseCausesReadReturn) {
    struct Server {
        hycast::SrvrSctpSock srvrSock;
        hycast::SctpSock     peerSock;
        Server(hycast::SrvrSctpSock& serverSock) : srvrSock{serverSock} {}
        void operator()() {
            peerSock = srvrSock.accept();
            uint32_t nbytes = peerSock.getSize();
            EXPECT_EQ(0, nbytes);
        }
    } server{srvrSock};
    std::thread srvrThread = std::thread([&server](){server();});
    clntSock.close();
    srvrThread.join();
}

/*
 * Tests whether an exception is thrown when the server's listening socket is
 * closed
 */
TEST_F(SocketTest, ServersCloseThrowsException) {
    struct Server {
        hycast::SrvrSctpSock serverSock;
        hycast::SctpSock     sock;
        Server(hycast::SrvrSctpSock& serverSock) : serverSock{serverSock} {}
        int run() {
            sock = serverSock.accept();
            sock.getSize();
            return 0;
        }
    } server{srvrSock};
    auto future = std::async([&server](){return server.run();});
    hycast::ClntSctpSock clientSock(srvrAddr, numStreams);
    server.serverSock.close();
    server.sock.close();
    EXPECT_THROW(future.get(), std::system_error);
}

#if 0
// Tests clean closing of receive socket
TEST_F(SocketTest, CleanReceiveClose) {
    hycast::InetSockAddr serverSockAddr(INET_ADDR, MY_PORT_NUM);
    hycast::SrvrSctpSock srvrSock(serverSockAddr, numStreams);
    struct Server {
        hycast::SrvrSctpSock srvrSock;
        hycast::SctpSock       peerSock;
        std::mutex           mutex;
        Server(hycast::SrvrSctpSock& srvrSock)
            : srvrSock{srvrSock}
            , peerSock{}
            , mutex{}
        {}
        int run() {
            mutex.lock();
            peerSock = srvrSock.accept();
            mutex.unlock();
            peerSock.getSize();
            return 0;
        }
    } server{srvrSock};
    server.mutex.lock();
    auto future = std::async(std::launch::async,
            [&server]{ return server.run(); });
    server.mutex.unlock();
    hycast::ClntSctpSock clientSock(serverSockAddr, numStreams);
    server.mutex.lock();
    ::sleep(1);
    server.peerSock.close();
    EXPECT_EQ(0, future.get());
}
#endif

// Tests send() and recv()
TEST_F(SocketTest, SendRecv) {
    startServer();
    startClient();
    stopClient();
    stopServer();
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
