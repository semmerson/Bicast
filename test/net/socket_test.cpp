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
#include "config.h"

#include "error.h"
#include "Thread.h"

#include <arpa/inet.h>
#include <atomic>
#include <cerrno>
#include <climits>
#include <condition_variable>
#include <cstring>
#include <fcntl.h>
#include <future>
#include <mutex>
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

        srvrAddr.sin_family = AF_INET;
        srvrAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        srvrAddr.sin_port = htons(38800);
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
    struct sockaddr_in      srvrAddr = {};
};

// Tests multicasting
TEST_F(SocketTest, Multicasting) {
    int recvSock = makeRecvSocket();
    std::thread recvThread([this,recvSock]{recv(recvSock);});
    int sendSock = makeSendSocket();
    std::thread sendThread([this,sendSock]{send(sendSock);});
    recvThread.join();
    sendThread.join();
    ::close(recvSock);
    ::close(sendSock);
}

#if 0
/*
 * Tests terminating a read on a socket by closing the socket.
 * THIS DOESN'T WORK
 */
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
		void stop() {
			::stop(sd);
		}
	} reader{recvSock};
    std::future<int> future = std::async([&reader]{return reader();});
    ::sleep(1);
	reader.stop();
	EXPECT_EQ(-2, future.get());
}
#endif

/**
 * A server that is stopped by polling a signaling pipe.
 */
class PollableServer {
	int             srvrSd; /// Server socket descriptor
    int             pipeFds[2];
public:
	std::atomic_bool threadStarted;
	std::atomic_bool acceptPollReturned;
	std::atomic_bool acceptReturned;
	std::atomic_bool readPollReturned;
	std::atomic_bool readReturned;
	PollableServer(struct sockaddr_in& srvrAddr)
		: srvrSd{::socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP)},
		  pipeFds{-1, -1},
		  threadStarted{false},
		  acceptPollReturned{false},
		  acceptReturned{false},
		  readPollReturned{false},
		  readReturned{false}
	{
		EXPECT_NE(-1, srvrSd);
		int status = ::bind(srvrSd,
				reinterpret_cast<struct sockaddr*>(&srvrAddr),
				sizeof(srvrAddr));
		EXPECT_EQ(0, status);
		status = ::listen(srvrSd, 5);
		EXPECT_EQ(0, status);
		status = pipe(pipeFds);
		EXPECT_EQ(0, status);
	}
	~PollableServer() {
		stop();
		::close(srvrSd);
		::close(pipeFds[0]);
		::close(pipeFds[1]);
	}
	ssize_t operator()() {
	    threadStarted = true;
		struct pollfd   pollfd[2];
		const int       stopIndex = 0;
		const int       sockIndex = 1;
		for (;;) {
            pollfd[stopIndex].fd = pipeFds[0]; // Works if write to pipeFds[1]
            pollfd[stopIndex].fd = pipeFds[1]; // Can result in hanging
            pollfd[stopIndex].events = POLLIN;
            pollfd[sockIndex].fd = srvrSd;
            pollfd[sockIndex].events = POLLIN;
            ::poll(pollfd, 2, -1); // -1 => indefinite wait
            acceptPollReturned = true;
            if (pollfd[stopIndex].revents)
                return -2;
            if (pollfd[sockIndex].revents & ~POLLIN)
                return -3;
            struct sockaddr clntAddr = {};
            socklen_t       clntAddrLen = sizeof(clntAddr);
            int peerSd = ::accept(srvrSd, &clntAddr, &clntAddrLen);
            acceptReturned = true;
            EXPECT_EQ(sizeof(struct sockaddr_in), clntAddrLen);
            EXPECT_TRUE(peerSd >= 0);
            pollfd[sockIndex].fd = peerSd;
            // Can hang here if signal is closing write-end of pipe
            ::poll(pollfd, 2, -1); // -1 => indefinite wait
            readPollReturned = true;
            if (pollfd[stopIndex].revents) {
                ::close(peerSd);
                return -4;
            }
            if (pollfd[sockIndex].revents & ~POLLIN)
                return -5;
            char buf[1];
            auto nbytes = read(peerSd, buf, sizeof(buf));
            readReturned = true;
            ::close(peerSd);
            return nbytes;
		}
	}
	void stop() {
        char buf[1];
        ::write(pipeFds[1], buf, sizeof(buf));
        ::close(pipeFds[1]); // Doesn't work for PollableReadTermination
	}
};

// Tests procedure for terminating an accept().
TEST_F(SocketTest, PollableAcceptTermination) {
	PollableServer server{srvrAddr};
    /*
     * For an unknown reason, the following doesn't work when std::async() is
     * used. It also prevents the Eclipse debugger from seeing both threads.
     */
    auto srvrThread = hycast::Thread([&]{server();});
    //::sleep(1);
    server.stop();
    srvrThread.join();
    if (server.threadStarted) {
        EXPECT_TRUE(server.acceptPollReturned);
        EXPECT_FALSE(server.readPollReturned);
    }
}

#if 0
// Tests procedure for terminating a socket read.
TEST_F(SocketTest, PollableReadTermination) {
	PollableServer server{srvrAddr};
    /*
     * For an unknown reason, the following doesn't work when std::async() is
     * used. It also prevents the Eclipse debugger from seeing both threads.
     */
    auto srvrThread = hycast::Thread([&]{server();});
    auto clntSock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
	EXPECT_NE(-1, clntSock);
	auto status = ::connect(clntSock,
			reinterpret_cast<struct sockaddr*>(&srvrAddr), sizeof(srvrAddr));
	EXPECT_EQ(0, status);
    //::sleep(1);
    server.stop();
    srvrThread.join();
    if (server.threadStarted) {
        EXPECT_TRUE(server.acceptPollReturned);
        if (server.acceptReturned) {
            EXPECT_TRUE(server.readPollReturned);
            EXPECT_FALSE(server.readReturned);
        }
    }
}
#endif

#if 0
// Tests procedure for terminating a socket read.
TEST_F(SocketTest, ReadTermination) {
    struct sockaddr_in srvrAddr = {};
	srvrAddr.sin_family = AF_INET;
	srvrAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	srvrAddr.sin_port = htons(38800);
	PollableServer server{srvrAddr};
    /*
     * For an unknown reason, the following doesn't work when std::async() is
     * used. It also prevents the Eclipse debugger from seeing both threads.
     */
#define USE_ASYNC 0
#if USE_ASYNC
    auto future = std::async([&server]{return server();});
#else
    //server.stop(); // Causes hanging if here
    std::atomic_long result;
    auto srvrThread = std::thread([&]{result = server();});
#endif
    auto clntSock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
	EXPECT_NE(-1, clntSock);
	auto status = ::connect(clntSock,
			reinterpret_cast<struct sockaddr*>(&srvrAddr), sizeof(srvrAddr));
	EXPECT_EQ(0, status);
    ::sleep(1);
    server.stop(); // Works if here
#if USE_ASYNC
    status = future.get();
    EXPECT_EQ(-3, status);
#else
    srvrThread.join();
    EXPECT_EQ(-3, result);
#endif
}
#endif

/**
 * A server that is stopped by canceling the thread on which it's executing.
 */
class CancellableServer {
	int srvrSd; /// Server socket descriptor
	int clntSd; /// Client socket descriptor
	std::mutex              mutex;
	std::condition_variable cond;
	std::thread::native_handle_type nativeHandle;
	static void acceptCleanup(void* arg)
	{
	    CancellableServer* obj = static_cast<CancellableServer*>(arg);
	    obj->acceptCleanupCalled = true;
	}
	static void readCleanup(void* arg)
	{
	    CancellableServer* obj = static_cast<CancellableServer*>(arg);
	    EXPECT_TRUE(obj->clntSd >= 0);
	    int status = ::close(obj->clntSd);
	    EXPECT_EQ(0, status);
	    obj->readCleanupCalled = true;
	}
public:
	std::atomic_bool started;
	std::atomic_bool accepted;
	std::atomic_bool acceptCleanupCalled;
	std::atomic_bool readCleanupCalled;
	CancellableServer(struct sockaddr_in& srvrAddr)
		: srvrSd{::socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP)},
		  clntSd{-1},
		  started{false},
		  accepted{false},
		  acceptCleanupCalled{false},
		  readCleanupCalled{false},
		  mutex{},
		  cond{},
		  nativeHandle{}
	{
		EXPECT_NE(-1, srvrSd);
		int status = ::bind(srvrSd,
				reinterpret_cast<struct sockaddr*>(&srvrAddr),
				sizeof(srvrAddr));
		EXPECT_EQ(0, status);
		status = ::listen(srvrSd, 5);
		EXPECT_EQ(0, status);
	}
	~CancellableServer() {
		::close(srvrSd);
	}
	void operator()() {
	    // A thread can be cancelled before it ever starts
	    {
	        std::lock_guard<decltype(mutex)> lock(mutex);
            started = true;
            nativeHandle = ::pthread_self();
            cond.notify_one();
	    }
	    THREAD_CLEANUP_PUSH(acceptCleanup, this);
            struct sockaddr clntAddr = {};
            socklen_t       clntAddrLen = sizeof(clntAddr);
            clntSd = ::accept(srvrSd, &clntAddr, &clntAddrLen);
            accepted = true;
            THREAD_CLEANUP_PUSH(readCleanup, this);
            EXPECT_EQ(sizeof(struct sockaddr_in), clntAddrLen);
            EXPECT_TRUE(clntSd >= 0);
                for (;;) {
                    char buf[1];
                    auto nbytes = read(clntSd, buf, sizeof(buf));
                }
            THREAD_CLEANUP_POP(true);
	    THREAD_CLEANUP_POP(false);
	}
	void stop()
	{
        std::unique_lock<decltype(mutex)> lock(mutex);
        while (!started)
            cond.wait(lock);
        ::pthread_cancel(nativeHandle);
	}
};

// Tests mechanism for terminating an `accept()`
TEST_F(SocketTest, AcceptTermination)
{
	CancellableServer server{srvrAddr};
    std::atomic_long result;
    auto srvrThread = hycast::Thread([&]{server();});
    srvrThread.cancel();
    srvrThread.join();
    if (server.started) {
        EXPECT_EQ(true, server.acceptCleanupCalled);
        EXPECT_EQ(false, server.accepted);
        EXPECT_EQ(false, server.readCleanupCalled);
    }
}

// Tests mechanism for terminating a read on a socket.
TEST_F(SocketTest, ReadTermination)
{
	CancellableServer server{srvrAddr};
    std::atomic_long result;
    auto srvrThread = hycast::Thread([&]{server();});
    auto clntSd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
	EXPECT_NE(-1, clntSd);
	auto status = ::connect(clntSd,
			reinterpret_cast<struct sockaddr*>(&srvrAddr), sizeof(srvrAddr));
	EXPECT_EQ(0, status);
	/*
	 * One or the other of the following increases the likelihood that the
	 * return from the `accept()` happens before the thread cancellation. It
	 * appears, therefore, that a successful `connect()` doesn't mean that the
	 * corresponding `accept()` has returned.
	 */
#if 1
    char buf[1];
    status = write(clntSd, buf, sizeof(buf));
    EXPECT_EQ(1, status);
#else
	::sleep(1);
#endif
    //srvrThread.cancel(); // Works
	server.stop(); // Works
    srvrThread.join();
    if (server.started) {
        EXPECT_EQ(true, server.acceptCleanupCalled);
        if (server.accepted)
            EXPECT_EQ(true, server.readCleanupCalled);
    }
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
