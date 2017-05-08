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
#include <atomic>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <future>
#include <poll.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

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

/*
 * Verifies procedure for terminating a read on a socket when the socket is
 * closed. THIS DOESN'T WORK
 */
#if 0
TEST_F(SocketTest, LocallyTerminatingRead)
{
    int recvSock = makeRecvSocket();
	struct Reader {
		int sd;
		Reader(int sd) : sd{sd} {}
		int operator()() {
			char buf[1];
			struct pollfd fd;
			fd.fd = sd;
			fd.events = POLLIN;
			::poll(&fd, 1, -1);
			if (fd.revents & (POLLHUP | POLLERR))
				return -2;
			return ::read(sd, buf, sizeof(buf));
		}
		void close() {
			::close(sd);
		}
	} reader{recvSock};
    std::future<int> future = std::async([&reader]{return reader();});
    ::sleep(1);
	reader.close();
	EXPECT_EQ(-2, future.get());
}
#endif

// Tests procedure for terminating read
TEST_F(SocketTest, ReadTermination) {
    struct sockaddr_in srvrAddr = {};
	srvrAddr.sin_family = AF_INET;
	srvrAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	srvrAddr.sin_port = htons(38800);
    struct Server {
        int             srvrSock;
        std::atomic_int signalFd;
        int             peerSock;
        Server(struct sockaddr_in& srvrAddr)
			: srvrSock{::socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP)},
			  signalFd{-1},
              peerSock{-1}
        {
        	EXPECT_NE(-1, srvrSock);
        	int status = ::bind(srvrSock,
        			reinterpret_cast<struct sockaddr*>(&srvrAddr),
					sizeof(srvrAddr));
        	EXPECT_EQ(0, status);
        	status = ::listen(srvrSock, 5);
        	EXPECT_EQ(0, status);
        	int fds[2];
        	status = pipe(fds);
        	EXPECT_EQ(0, status);
        	signalFd = fds[1];
        	::close(fds[0]);
        }
        ~Server() {
        	::close(srvrSock);
        }
        ssize_t operator()() {
        	struct sockaddr clntAddr = {};
        	socklen_t       clntAddrLen = sizeof(clntAddr);
            peerSock = ::accept(srvrSock, &clntAddr, &clntAddrLen);
            EXPECT_EQ(sizeof(struct sockaddr_in), clntAddrLen);
            EXPECT_TRUE(peerSock >= 0);
			struct pollfd pollfd[2];
			pollfd[0].fd = peerSock;
			pollfd[0].events = POLLIN;
			pollfd[1].fd = signalFd;
			pollfd[1].events = POLLIN;
			::poll(pollfd, 2, -1);
			if (pollfd[1].revents & (POLLHUP | POLLERR | POLLNVAL))
				return -2;
            char buf[1];
			auto nbytes = read(peerSock, buf, sizeof(buf));
            ::close(peerSock);
            return nbytes;
        }
        void close() {
        	::close(signalFd);
        }
    } server{srvrAddr};
    /*
     * For an unknown reason, the following doesn't work when std::async() is
     * used. It also prevents the Eclipse debugger from seeing both threads.
     */
#define USE_ASYNC 0
#if USE_ASYNC
    auto future = std::async([&server]{return server();});
#else
    std::atomic_long result;
    auto srvrThread = std::thread([&]{result = server();});
#endif
    auto clntSock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
	ASSERT_NE(-1, clntSock);
	auto status = ::connect(clntSock,
			reinterpret_cast<struct sockaddr*>(&srvrAddr), sizeof(srvrAddr));
	ASSERT_EQ(0, status);
    ::sleep(1);
    server.close();
#if USE_ASYNC
    status = future.get();
    ASSERT_EQ(-2, status);
#else
    srvrThread.join();
    ASSERT_EQ(-2, result);
#endif
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
