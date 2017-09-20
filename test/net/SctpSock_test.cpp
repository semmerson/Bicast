/**
 * This file tests the class `SctpSock`.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYRIGHT in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Socket_test.cpp
 * @author: Steven R. Emmerson
 */

#include "error.h"
#include "InetSockAddr.h"
#include "Interface.h"
#include "SctpSock.h"

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

void runClient(hycast::SctpSock sock)
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
}

// The fixture for testing class Socket.
class SctpTest : public ::testing::Test
{
protected:
    SctpTest()
    {
        sock1 = ::socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
        sock2 = ::socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
    }

    virtual ~SctpTest() {
        close(sock1);
        close(sock2);
    }

    // Objects declared here can be used by all tests in the test case for Socket.
    const in_port_t         MY_PORT_NUM = 38800;
    const int               numStreams = 5;
    const hycast::Interface nic{"lo"};
    hycast::InetSockAddr    srvrAddr{nic.getInetAddr(AF_INET), MY_PORT_NUM};
    int                     sock1, sock2;
    std::thread             recvThread;
    std::thread             sendThread;

    bool is_open(int sock)
    {
        struct stat statbuf;
        return fstat(sock, &statbuf) == 0;
    }

    void startServer()
    {
        // Work done here so that socket is being listened to when client starts
        hycast::SrvrSctpSock    srvrSock{srvrAddr, numStreams};
        srvrSock.listen();
        recvThread = std::thread(runServer, srvrSock);
    }

    void startClient()
    {
        hycast::SctpSock        clntSock{srvrAddr, numStreams};
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
TEST_F(SctpTest, InvalidArgument) {
    EXPECT_THROW(hycast::SctpSock(srvrAddr, -1), std::exception);
    EXPECT_THROW(hycast::SrvrSctpSock(srvrAddr, -1), std::exception);
}

// Tests destruction
TEST_F(SctpTest, CloseOnDestruction) {
    int sd;
    {
        hycast::SrvrSctpSock s{srvrAddr};
        sd = s.getSock();
        EXPECT_EQ(true, is_open(sd));
    }
    EXPECT_EQ(false, is_open(sd));
}

// Tests copy-construction
TEST_F(SctpTest, CopyConstruction) {
    hycast::SrvrSctpSock s1{srvrAddr};
    s1.listen();
    auto sd1 = s1.getSock();
    EXPECT_TRUE(is_open(sd1));
    hycast::SrvrSctpSock s2(s1);
    s1.~SrvrSctpSock();
    EXPECT_TRUE(sd1);
    EXPECT_EQ(sd1, s2.getSock());
    s2.~SrvrSctpSock();
    EXPECT_FALSE(is_open(sd1));
}

// Tests move-assignment
TEST_F(SctpTest, MoveAssignment) {
    hycast::SrvrSctpSock s1 = hycast::SrvrSctpSock{srvrAddr};
    s1.listen();
    auto sd = s1.getSock();
    EXPECT_TRUE(is_open(sd));
    s1.~SrvrSctpSock();
    EXPECT_FALSE(is_open(sd));
}

// Tests copy-assignment to empty instance
TEST_F(SctpTest, CopyAssignmentToEmpty) {
    hycast::SrvrSctpSock s1{srvrAddr};
    auto sd1 = s1.getSock();
    EXPECT_TRUE(is_open(sd1));
    hycast::SrvrSctpSock s2{};
    EXPECT_FALSE(is_open(s2.getSock()));
    s2 = s1;
    EXPECT_TRUE(is_open(sd1));
    EXPECT_EQ(sd1, s2.getSock());
    s1.~SrvrSctpSock();
    EXPECT_TRUE(is_open(sd1));
    s2.~SrvrSctpSock();
    EXPECT_FALSE(is_open(sd1));
}

// Tests copy-assignment to self
TEST_F(SctpTest, CopyAssignmentToSelf) {
    hycast::SrvrSctpSock s1{srvrAddr};
    s1.listen();
    auto sd1 = s1.getSock();
    s1 = s1;
    EXPECT_EQ(sd1, s1.getSock());
    EXPECT_TRUE(is_open(sd1));
}

// Tests setting send buffer size
TEST_F(SctpTest, SendBufferSize) {
    hycast::SrvrSctpSock s{srvrAddr};
    auto size = s.getSendBufSize();
    s.setSendBufSize(size+1024);
    EXPECT_TRUE(size < s.getSendBufSize());
}

// Tests setting receive buffer size
TEST_F(SctpTest, ReceiveBufferSize) {
    hycast::SrvrSctpSock s{srvrAddr};
    auto size = s.getRecvBufSize();
    s.setRecvBufSize(size+1024);
    EXPECT_TRUE(size < s.getRecvBufSize());
}

// Tests equality operator
TEST_F(SctpTest, EqualityOperator) {
    hycast::SrvrSctpSock s1{srvrAddr};
    EXPECT_EQ(true, s1 == s1);
    hycast::SrvrSctpSock s2{};
    EXPECT_EQ(false, s2 == s1);
}

// Tests to_string()
TEST_F(SctpTest, ToString) {
    hycast::SrvrSctpSock s1{srvrAddr};
    //std::cout << s1.to_string() << '\n';
    std::string expect{"{sd=" + std::to_string(s1.getSock()) +
            ", numStreams=" + std::to_string(s1.getNumStreams()) + "}"};
    EXPECT_STREQ(expect.data(), s1.to_string().data());
}

// Tests whether a read() on a socket returns when the remote end is closed.
TEST_F(SctpTest, RemoteCloseCausesReadReturn) {
    struct Server {
        hycast::SrvrSctpSock srvrSock;
        hycast::SctpSock     peerSock;
        Server(hycast::SrvrSctpSock&& serverSock) : srvrSock{serverSock} {}
        void operator()() {
            srvrSock.listen();
            peerSock = srvrSock.accept();
            uint32_t nbytes = peerSock.getSize();
            EXPECT_EQ(0, nbytes);
        }
    } server{hycast::SrvrSctpSock{srvrAddr, numStreams}};
    std::thread srvrThread = std::thread(server);
    {
        hycast::SctpSock clntSock{srvrAddr, numStreams};
    }
    srvrThread.join();
}

#if 0
// Tests clean closing of receive socket
// THIS DOESN'T WORK. Closing a socket doesn't cause a read() on it to return
TEST_F(SctpTest, CleanReceiveClose) {
    hycast::SrvrSctpSock srvrSock(srvrAddr, numStreams);
    struct Server {
        hycast::SrvrSctpSock srvrSock;
        hycast::SctpSock     peerSock;
        std::mutex           mutex;
        Server(hycast::SrvrSctpSock& srvrSock)
            : srvrSock{srvrSock}
            , peerSock{}
            , mutex{}
        {}
        int operator()()
        {
            mutex.lock();
            peerSock = srvrSock.accept();
            mutex.unlock();
            peerSock.getSize();
            return 0;
        }
    } server{srvrSock};
    server.mutex.lock();
    auto future = std::async(std::launch::async,
            [&server]{ return server(); });
    server.mutex.unlock();
    hycast::SctpSock clientSock(srvrAddr, numStreams);
    server.mutex.lock();
    ::sleep(1);
    server.peerSock.close();
    EXPECT_EQ(0, future.get());
}
#endif

#if 0
/*
 * Tests whether an exception is thrown when the server's listening socket is
 * closed
 */
TEST_F(SctpTest, ServersCloseThrowsException) {
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

// Tests send() and recv()
TEST_F(SctpTest, SendRecv) {
    startServer();
    startClient();
    stopClient();
    stopServer();
}
#endif

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
