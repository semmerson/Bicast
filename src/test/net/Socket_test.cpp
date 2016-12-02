/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYRIGHT in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Socket_test.cpp
 * @author: Steven R. Emmerson
 *
 * This file tests class `Socket`.
 */

#include "ClientSocket.h"
#include "InetSockAddr.h"
#include "ServerSocket.h"
#include "Socket.h"

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

static const unsigned numStreams = 5;

void runServer(hycast::ServerSocket serverSock)
{
    hycast::Socket connSock(serverSock.accept());
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

void runClient(const int port)
{
    // Test Socket::send() and Socket::recv()
    hycast::InetSockAddr sockAddr("127.0.0.1", port);
    hycast::ClientSocket sock(sockAddr, numStreams);
    for (int i = 0; i < 100; ++i) {
        unsigned outStream = i % numStreams;
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
}

// The fixture for testing class Socket.
class SocketTest : public ::testing::Test {
 protected:
  // You can remove any or all of the following functions if its body
  // is empty.

  SocketTest() {
    // You can do set-up work for each test here.
    sock1 = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
    sock2 = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
  }

  virtual ~SocketTest() {
    // You can do clean-up work that doesn't throw exceptions here.
      close(sock1);
      close(sock2);
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

    // Objects declared here can be used by all tests in the test case for Socket.
    int sock1, sock2;
    const int MY_PORT_NUM = 38800;
    std::thread recvThread;
    std::thread sendThread;

    bool is_open(int sock)
    {
        struct stat statbuf;
        return fstat(sock, &statbuf) == 0;
    }

    void startServer()
    {
        // Work done here so that socket is being listened to when client starts
        hycast::InetSockAddr sockAddr("127.0.0.1", MY_PORT_NUM);
        hycast::ServerSocket sock(sockAddr, numStreams);
        recvThread = std::thread(runServer, sock);
    }

    void startClient()
    {
        sendThread = std::thread(runClient, MY_PORT_NUM);
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
    EXPECT_THROW(hycast::Socket s(-1), std::invalid_argument);
}

// Tests destruction
TEST_F(SocketTest, CloseOnDestruction) {
    hycast::Socket s(sock1);
    EXPECT_EQ(true, is_open(sock1));
    s.~Socket();
    EXPECT_EQ(false, is_open(sock1));
}

// Tests copy-construction
TEST_F(SocketTest, CopyConstruction) {
    hycast::Socket s1(sock1);
    EXPECT_EQ(true, is_open(sock1));
    hycast::Socket s2(s1);
    s1.~Socket();
    EXPECT_EQ(true, is_open(sock1));
    s2.~Socket();
    EXPECT_EQ(false, is_open(sock1));
}

// Tests move-assignment
TEST_F(SocketTest, MoveAssignment) {
    hycast::Socket s1 = hycast::Socket(sock1);
    EXPECT_EQ(true, is_open(sock1));
    s1.~Socket();
    EXPECT_EQ(false, is_open(sock1));
}

// Tests copy-assignment to empty instance
TEST_F(SocketTest, CopyAssignmentToEmpty) {
    hycast::Socket s1(sock1);
    EXPECT_EQ(true, is_open(sock1));
    hycast::Socket s2;
    s2 = s1;
    EXPECT_EQ(true, is_open(sock1));
    s1.~Socket();
    EXPECT_EQ(true, is_open(sock1));
    s2.~Socket();
    EXPECT_EQ(false, is_open(sock1));
}

// Tests copy-assignment to non-empty instance
TEST_F(SocketTest, CopyAssignmentToNonEmpty) {
    hycast::Socket s1(sock1);
    EXPECT_EQ(true, is_open(sock1));
    hycast::Socket s2(sock2);
    EXPECT_EQ(true, is_open(sock2));
    s2 = s1;
    EXPECT_EQ(true, is_open(sock1));
    EXPECT_EQ(false, is_open(sock2));
    s1.~Socket();
    EXPECT_EQ(true, is_open(sock1));
    s2.~Socket();
    EXPECT_EQ(false, is_open(sock1));
}

// Tests copy-assignment to self
TEST_F(SocketTest, CopyAssignmentToSelf) {
    hycast::Socket s1(sock1);
    EXPECT_EQ(true, is_open(sock1));
    s1 = s1;
    EXPECT_EQ(true, is_open(sock1));
}

// Tests equality operator
TEST_F(SocketTest, EqualityOperator) {
    hycast::Socket s1(sock1);
    EXPECT_EQ(true, s1 == s1);
    hycast::Socket s2(sock2);
    EXPECT_EQ(false, s2 == s1);
}

// Tests to_string()
TEST_F(SocketTest, ToString) {
    hycast::Socket s1(sock1);
    EXPECT_STREQ((std::string("SocketImpl{sock=") + std::to_string(sock1) + "}").data(),
            s1.to_string().data());
}

// Tests whether a read() on a socket returns when the remote end is closed.
TEST_F(SocketTest, RemoteCloseCausesReadReturn) {
    hycast::InetSockAddr serverSockAddr("127.0.0.1", MY_PORT_NUM);
    hycast::ServerSocket serverSock(serverSockAddr, numStreams);
    struct Server {
        hycast::ServerSocket serverSock;
        hycast::Socket       sock;
        Server(hycast::ServerSocket& serverSock) : serverSock{serverSock} {}
        void run() {
            sock = serverSock.accept();
            uint32_t nbytes = sock.getSize();
            EXPECT_EQ(0, nbytes);
        }
    } server{serverSock};
    std::thread serverThread = std::thread([&server](){server.run();});
    hycast::ClientSocket clientSock(serverSockAddr, numStreams);
    clientSock.close();
    serverThread.join();
    EXPECT_TRUE(true);
}

// Tests whether an exception is thrown when the local socket is closed
TEST_F(SocketTest, LocalCloseThrowsException) {
    hycast::InetSockAddr serverSockAddr("127.0.0.1", MY_PORT_NUM);
    hycast::ServerSocket serverSock(serverSockAddr, numStreams);
    struct Server {
        hycast::ServerSocket serverSock;
        hycast::Socket       sock;
        Server(hycast::ServerSocket& serverSock) : serverSock{serverSock} {}
        int run() {
            sock = serverSock.accept();
            sock.getSize();
            return 0;
        }
    } server{serverSock};
    auto future = std::async([&server](){return server.run();});
    hycast::ClientSocket clientSock(serverSockAddr, numStreams);
    server.serverSock.close();
    server.sock.close();
    EXPECT_THROW(future.get(), std::system_error);
}

#if 0
// Tests clean closing of receive socket
TEST_F(SocketTest, CleanReceiveClose) {
    hycast::InetSockAddr serverSockAddr("127.0.0.1", MY_PORT_NUM);
    hycast::ServerSocket serverSock(serverSockAddr, numStreams);
    struct Server {
        hycast::ServerSocket serverSock;
        hycast::Socket       sock;
        std::mutex           mutex;
        Server(hycast::ServerSocket& serverSock)
            : serverSock{serverSock}
            , sock{}
            , mutex{}
        {}
        int run() {
            mutex.lock();
            sock = serverSock.accept();
            mutex.unlock();
            sock.getSize();
            return 0;
        }
    } server{serverSock};
    server.mutex.lock();
    auto future = std::async(std::launch::async,
            [&server]{ return server.run(); });
    server.mutex.unlock();
    hycast::ClientSocket clientSock(serverSockAddr, numStreams);
    server.mutex.lock();
    ::sleep(1);
    server.sock.close();
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
