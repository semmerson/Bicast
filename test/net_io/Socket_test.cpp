/**
 * This file tests class `Socket`.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *       File: Socket_test.cpp
 * Created On: May 17, 2019
 *     Author: Steven R. Emmerson
 */
#include "config.h"

#include "error.h"
#include "Socket.h"

#include <gtest/gtest.h>
#include <condition_variable>
#include <mutex>
#include <signal.h>
#include <thread>

#undef USE_SIGTERM

namespace {

#ifdef USE_SIGTERM
static void signal_handler(int const sig)
{}
#endif

/// The fixture for testing class `Socket`
class SocketTest : public ::testing::Test
{
protected:
    hycast::SockAddr        srvrAddr;
    std::mutex              mutex;
    std::condition_variable cond;
    bool                    srvrReady;
    std::thread             srvrThread;
    hycast::TcpSrvrSock     acceptSock; ///< Server's `::accept()` socket
    hycast::TcpSock         srvrSock;       ///< Socket from `::accept()`

    // You can remove any or all of the following functions if its body
    // is empty.

    SocketTest()
        : srvrAddr{"127.0.0.1:38800"} // Don't use "localhost" to enable comparison
        , mutex{}
        , cond{}
        , srvrReady{false}
        , srvrThread()
        , acceptSock()
        , srvrSock()
    {
        // You can do set-up work for each test here.
    }

    virtual ~SocketTest()
    {
        // You can do clean-up work that doesn't throw exceptions here.
    }

    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:

    virtual void SetUp()
    {
        // Code here will be called immediately after the constructor (right
        // before each test).
#ifdef USE_SIGTERM
        struct sigaction sigact;
        (void)::sigemptyset(&sigact.sa_mask);
        sigact.sa_flags = 0;
        sigact.sa_handler = signal_handler;
        (void)sigaction(SIGTERM, &sigact, NULL);
#endif
    }

    virtual void TearDown()
    {
        // Code here will be called immediately after each test (right
        // before the destructor).
#ifdef USE_SIGTERM
        struct sigaction sigact;
        (void)::sigemptyset(&sigact.sa_mask);
        sigact.sa_flags = 0;
        sigact.sa_handler = SIG_DFL;
        (void)sigaction(SIGTERM, &sigact, NULL);
#endif
    }

    // Objects declared here can be used by all tests in the test case for Socket.

public:
    void runServer()
    {
        {
            std::lock_guard<decltype(mutex)> lock{mutex};
            srvrReady = true;
            cond.notify_one();
        }

        try {
            srvrSock = acceptSock.accept();

            if (srvrSock) {
                for (;;) {
                    int readInt;

                    srvrSock.read(&readInt, sizeof(readInt));
                    srvrSock.write(&readInt, sizeof(readInt));
                }
            }
        }
        catch (std::exception const& ex) {
            std::cout << "runserver(): Exception caught\n";
        }
    }

    void startServer()
    {
        acceptSock = hycast::TcpSrvrSock(srvrAddr);
        srvrThread = std::thread(&SocketTest::runServer, this);

        // Necessary because `ClntSock` constructor throws if `connect()` fails
        std::unique_lock<decltype(mutex)> lock{mutex};
        while (!srvrReady)
            cond.wait(lock);
    }
};

// Tests copy construction
TEST_F(SocketTest, CopyConstruction)
{
    hycast::TcpSrvrSock srvrSock{srvrAddr};
    hycast::TcpSrvrSock sock(srvrSock);
}

// Tests setting the Nagle algorithm
TEST_F(SocketTest, SettingNagle)
{
    hycast::TcpSrvrSock srvrSock(srvrAddr);

    EXPECT_TRUE(&srvrSock.setDelay(false) == &srvrSock);
}

// Tests server-socket construction
TEST_F(SocketTest, ServerConstruction)
{
    hycast::TcpSrvrSock srvrSock(srvrAddr);

    hycast::SockAddr sockAddr(srvrSock.getLclAddr());
    LOG_DEBUG("%s", sockAddr.to_string().c_str());
    EXPECT_TRUE(!(srvrAddr < sockAddr) && !(sockAddr < srvrAddr));
}

#if 0
// Calling shutdown() on a TCP connection doesn't cause poll() to return

// Tests shutdown of socket while reading
TEST_F(SocketTest, ReadShutdown)
{
    startServer();

    hycast::TcpClntSock clntSock(srvrAddr);

    ::sleep(1);
    acceptSock.shutdown();
    srvrThread.join();
}
#endif

// Tests canceling the server thread while accept() is executing
TEST_F(SocketTest, CancelAccept)
{
    startServer();

#ifdef USE_SIGTERM
    ::pthread_kill(srvrThread.native_handle(), SIGTERM);
#else
    ::pthread_cancel(srvrThread.native_handle());
#endif
    srvrThread.join();
}

// Tests shutting down the server's accept-socket
TEST_F(SocketTest, ShutdownAccept)
{
    startServer();
    acceptSock.shutdown();
    srvrThread.join();
}

// Tests canceling the server thread while read() is executing
TEST_F(SocketTest, CancelRead)
{
    startServer();

    hycast::TcpClntSock clntSock(srvrAddr);

    ::usleep(100000);
#ifdef USE_SIGTERM
    ::pthread_kill(srvrThread.native_handle(), SIGTERM);
#else
    ::pthread_cancel(srvrThread.native_handle());
#endif
    srvrThread.join();
}

// Tests shutting down the server's socket while read() is executing
TEST_F(SocketTest, shutdownRead)
{
    startServer();

    hycast::TcpClntSock clntSock(srvrAddr);

    ::usleep(100000);
    srvrSock.shutdown();
    srvrThread.join();
}

// Tests shutting down the server's socket while write() is executing
TEST_F(SocketTest, shutdownWrite)
{
    startServer();

    hycast::TcpClntSock clntSock(srvrAddr);

    int writeInt = 0xff00;
    clntSock.write(&writeInt, sizeof(writeInt));

    ::usleep(100000);
    srvrSock.shutdown();
    srvrThread.join();
}

// Tests round-trip scalar exchange
TEST_F(SocketTest, ScalarExchange)
{
    startServer();

    hycast::TcpClntSock clntSock(srvrAddr);
    int              writeInt = 0xff00;
    int              readInt = ~writeInt;

    clntSock.write(&writeInt, sizeof(writeInt));
    clntSock.read(&readInt, sizeof(readInt));

    EXPECT_EQ(writeInt, readInt);

    ::pthread_cancel(srvrThread.native_handle());
    srvrThread.join();
}

// Tests round-trip vector exchange
TEST_F(SocketTest, VectorExchange)
{
    startServer();

    hycast::TcpClntSock clntSock(srvrAddr);
    int                 writeInt[2] = {0xff00, 0x00ff};
    int                 readInt[2] = {0};
    struct iovec        iov[2];

    clntSock.write(writeInt, sizeof(writeInt));
    clntSock.read(readInt, sizeof(writeInt));

    EXPECT_EQ(writeInt[0], readInt[0]);
    EXPECT_EQ(writeInt[1], readInt[1]);

    ::pthread_cancel(srvrThread.native_handle());
    srvrThread.join();
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
