/**
 * This file tests class `Socket`.
 *
 *       File: Socket_test.cpp
 * Created On: May 17, 2019
 *     Author: Steven R. Emmerson
 *
 *    Copyright 2021 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "config.h"

#include "error.h"
#include "logging.h"
#include "Socket.h"

#include <gtest/gtest.h>
#include <condition_variable>
#include <mutex>
#include <signal.h>
#include <thread>

namespace {

using namespace bicast;

/// The fixture for testing class `Socket`
class SocketTest : public ::testing::Test
{
protected:
    typedef enum {
        INIT = 0,
        LISTENING = 0x1,
        CONNECTED = 0x3,
        READ_SOMETHING = 0x7,
    } State;
    SockAddr                srvrAddr;
    std::mutex              mutex;
    std::condition_variable cond;
    bool                    srvrReady;
    std::thread             srvrThread;
    State                   state;

    // You can remove any or all of the following functions if its body
    // is empty.

    SocketTest()
        : srvrAddr{"localhost:38800"}
        , mutex{}
        , cond{}
        , srvrReady{false}
        , srvrThread()
        , state(INIT)
    {
        // You can do set-up work for each test here.
    }

    void setState(const State state) {
        std::lock_guard<decltype(mutex)> lock{mutex};
        this->state = state;
        cond.notify_one();
    }

    void waitForState(const State nextState)
    {
        std::unique_lock<decltype(mutex)> lock{mutex};
        while (state != nextState)
            cond.wait(lock);
    }

    // Objects declared here can be used by all tests in the test case for Socket.

    void runServer(TcpSrvrSock& lstnSock,
                   TcpSock&    srvrSock)
    {
        try {
            srvrSock = lstnSock.accept();
            setState(CONNECTED);

            if (srvrSock) {
                for (;;) {
                    uint8_t value;

                    if (!srvrSock.read(value))
                        break;
                    setState(READ_SOMETHING);
                    if (!srvrSock.write(value))
                        break;
                }
            }
        }
        catch (std::exception const& ex) {
            LOG_ERROR(ex, "Server failure");
        }
    }

    void startServer(TcpSrvrSock& lstnSock,
                     TcpSock&     srvrSock)
    {
        lstnSock = TcpSrvrSock(srvrAddr);
        setState(LISTENING);
        srvrThread = std::thread(&SocketTest::runServer, this, std::ref(lstnSock), std::ref(srvrSock));
    }

    template<typename VALUE>
    void scalarXchg(
            Socket& clntSock,
            VALUE&  writeValue) {
        VALUE readValue = ~writeValue;
        clntSock.write(writeValue);
        clntSock.read(readValue);
        EXPECT_EQ(writeValue, readValue);
    }
};

// Tests copy construction
TEST_F(SocketTest, CopyConstruction)
{
    TcpSrvrSock lstnSock{srvrAddr};
    TcpSrvrSock sock(lstnSock);
}

// Tests setting the Nagle algorithm
TEST_F(SocketTest, SettingNagle)
{
    TcpSrvrSock lstnSock(srvrAddr);

    EXPECT_TRUE(&lstnSock.setDelay(false) == &lstnSock);
}

// Tests server-socket construction
TEST_F(SocketTest, ServerConstruction)
{
    TcpSrvrSock lstnSock(srvrAddr);
}

// Tests canceling the server thread after `::listen()` has been called
TEST_F(SocketTest, CancelListening)
{
    TcpSrvrSock lstnSock;
    TcpSock     srvrSock;

    startServer(lstnSock, srvrSock);

    ::pthread_cancel(srvrThread.native_handle());
    srvrThread.join();
}

// Tests shutting down the server's listening-socket
TEST_F(SocketTest, ShutdownAcceptSocket)
{
    TcpSrvrSock lstnSock{};
    TcpSock     srvrSock{};

    startServer(lstnSock, srvrSock);
    ::sleep(1);
    lstnSock.shutdown();
    srvrThread.join();
}

// Tests canceling the server thread while read() is executing
TEST_F(SocketTest, CancelServerReading)
{
    TcpSrvrSock lstnSock{};
    TcpSock     srvrSock{};

    //LOG_DEBUG("Starting server");
    startServer(lstnSock, srvrSock);

    //LOG_DEBUG("Constructing client socket");
    TcpClntSock clntSock(srvrAddr);
    //LOG_DEBUG("Writing to client socket");
    auto success = clntSock.write(true);
    EXPECT_TRUE(success);

    //waitForState(READ_SOMETHING);
    //LOG_DEBUG("Canceling server thread");
    ::pthread_cancel(srvrThread.native_handle());
    //LOG_DEBUG("Joining server thread");
    srvrThread.join();
}

// Tests shutting down the server's socket
TEST_F(SocketTest, ShutdownServerSocket)
{
    TcpSrvrSock lstnSock;
    TcpSock     srvrSock;

    startServer(lstnSock, srvrSock);
    waitForState(LISTENING);

    TcpClntSock clntSock(srvrAddr);
    clntSock.write(true);

    waitForState(READ_SOMETHING);
    srvrSock.shutdown(); // Sends FIN to client's socket, but too late
    srvrThread.join();

    //sleep(1); // Even if this is enabled

    // The following amount is necessary for an EOF
    char bytes[5000000];
    //char bytes[1]; // Not enough even if the sleep is enabled
    ASSERT_EQ(false, clntSock.write(bytes, sizeof(bytes)));
}

// Tests shutting down the client's socket
TEST_F(SocketTest, ShutdownClientSocket)
{
    TcpSrvrSock lstnSock;
    TcpSock     srvrSock;

    startServer(lstnSock, srvrSock);
    waitForState(LISTENING);

    TcpClntSock clntSock(srvrAddr);
    EXPECT_EQ(true, clntSock.write(true));

    waitForState(READ_SOMETHING);
    clntSock.shutdown(); // Sends FIN to server's socket
    EXPECT_EQ(false, clntSock.write(true));

    srvrThread.join();
}

// Tests round-trip scalar exchange
TEST_F(SocketTest, ScalarExchange)
{
    TcpSrvrSock lstnSock;
    TcpSock     srvrSock;

    startServer(lstnSock, srvrSock);
    TcpClntSock clntSock(srvrAddr);

    bool     boolean = true;
    scalarXchg<bool>(clntSock, boolean);

    uint8_t     uint8 = 0xff;
    scalarXchg<uint8_t>(clntSock, uint8);

    uint16_t     uint16 = 0xffff;
    scalarXchg<uint16_t>(clntSock, uint16);

    uint32_t     uint32 = 0xffffffff;
    scalarXchg<uint32_t>(clntSock, uint32);

    uint64_t     uint64 = 0xffffffffffffffff;
    scalarXchg<uint64_t>(clntSock, uint64);

    ::pthread_cancel(srvrThread.native_handle());
    srvrThread.join();
}

// Tests round-trip I/O-vector exchange
TEST_F(SocketTest, VectorExchange)
{
    TcpSrvrSock lstnSock;
    TcpSock     srvrSock;

    startServer(lstnSock, srvrSock);

    TcpClntSock clntSock(srvrAddr);
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

static void myTerminate()
{
    if (!std::current_exception()) {
        LOG_FATAL("terminate() called without an active exception");
    }
    else {
        LOG_FATAL("terminate() called with an active exception");
        try {
            std::rethrow_exception(std::current_exception());
        }
        catch (const std::exception& ex) {
            LOG_FATAL(ex);
        }
        catch (...) {
            LOG_FATAL("Exception is unknown");
        }
    }
    abort();
}

int main(int argc, char **argv) {
  /*
   * Ignore SIGPIPE so that writing to a shut down socket doesn't terminate the
   * process (the return-value from write() is always checked).
   */
  struct sigaction sigact;
  sigact.sa_handler = SIG_IGN;
  sigemptyset(&sigact.sa_mask);
  sigact.sa_flags = 0;
  (void)sigaction(SIGPIPE, &sigact, NULL);

  log_setName(::basename(argv[0]));
  log_setLevel(LogLevel::DEBUG);
  //LOG_ERROR

  std::set_terminate(&myTerminate);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
