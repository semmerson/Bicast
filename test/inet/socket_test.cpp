/**
 * This file tests BSD TCP sockets.
 *
 *       File: socket_test.cpp
 * Created On: May 26, 2021
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
#include "SockAddr.h"

#include <gtest/gtest.h>
#include <condition_variable>
#include <mutex>
#include <signal.h>
#include <sys/socket.h>
#include <thread>

namespace {

using namespace bicast;

using Mutex = std::mutex;
using Guard = std::lock_guard<Mutex>;
using Lock  = std::unique_lock<Mutex>;
using Cond  = std::condition_variable;

/// The fixture for testing class `Socket`
class SocketTest : public ::testing::Test
{
protected:
    typedef enum {
        INIT           =   0x0,
        ACCEPTING      =   0x1,
        CLIENT_WRITING =   0x2,
        SERVER_READING =   0x4,
        SERVER_WRITING =   0x8,
        CLIENT_READING =  0x10,
        CLIENT_WRITE   =  0x20,
        SERVER_READ    =  0x40,
        SERVER_WRITE   =  0x80,
        CLIENT_READ    = 0x100,
        CONTINUE       = CLIENT_WRITE | SERVER_READ | SERVER_WRITE | CLIENT_READ
    } State;
    Mutex            mutex;
    Cond             cond;
    SockAddr         srvrAddr;
    std::thread      srvrThread;
    std::thread      clntThread;
    State            state;
    int              lstnSock;
    int              clntSock;
    int              srvrSock;

    // You can remove any or all of the following functions if its body
    // is empty.

    SocketTest()
        : mutex{}
        , cond{}
        , srvrAddr("127.0.0.1:38800")
        , srvrThread()
        , clntThread()
        , state(INIT)
        , lstnSock(-1)
        , clntSock(-1)
        , srvrSock(-1)
    {}

    void set(const State state) {
        Guard guard{mutex};
        this->state = static_cast<State>(this->state | state);
        cond.notify_one();
    }

    void unset(const State state) {
        Guard guard{mutex};
        this->state = static_cast<State>(this->state & ~state);
        cond.notify_one();
    }

    void waitUntilSet(const State state)
    {
        Lock lock{mutex};
        while ((this->state & state) != state) // precedence: "&" < "==" == "!="
            cond.wait(lock);
    }

    // Objects declared here can be used by all tests in the test case for Socket.

    void runServer()
    {
        try {
            struct sockaddr_in rmtAddr;
            socklen_t          addrLen = sizeof(rmtAddr);
            set(ACCEPTING);
            srvrSock = ::accept(lstnSock, reinterpret_cast<struct sockaddr*>(&rmtAddr), &addrLen);
            if (srvrSock == -1)
                throw SYSTEM_ERROR("accept() failure");

            try {
                for (;;) {
                    uint8_t byte;

                    set(SERVER_READING);
                    waitUntilSet(SERVER_READ);
                    const auto nbytes = ::read(srvrSock, &byte, sizeof(byte));
                    if (nbytes == 0) {
                        LOG_NOTE("Read EOF on socket %d", srvrSock);
                        break;
                    }
                    if (nbytes == -1)
                        throw SYSTEM_ERROR("read() failure");

                    set(SERVER_WRITING);
                    waitUntilSet(SERVER_WRITE);
                    if (::write(srvrSock, &byte, sizeof(byte)) != 1)
                        throw SYSTEM_ERROR("write() failure");
                }
            } // `srvrSock` allocated
            catch (const std::exception& ex) {
                ::close(srvrSock);
                srvrSock = -1;
                LOG_ERROR(ex, "Servlet failure");
            }
            ::close(srvrSock);
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex, "Server failure");
        }
    }

    void startServer() {
        // Normally, this `try` would be in the caller
        try {
            lstnSock = srvrAddr.getInetAddr().socket(SOCK_STREAM);

            try {
                const int enable = 1;
                if (::setsockopt(lstnSock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)))
                    throw SYSTEM_ERROR("Couldn't set SO_REUSEADDR on socket " +
                            std::to_string(lstnSock) + ", address " + srvrAddr.to_string());

                srvrAddr.bind(lstnSock);

                if (::listen(lstnSock, 0))
                    throw SYSTEM_ERROR("listen() failure: {sock=" + std::to_string(lstnSock) + "}");

                srvrThread = std::thread(&SocketTest::runServer, this);
            } // `lstnSock` allocated
            catch (const std::exception& ex) {
                ::close(lstnSock);
                throw;
            }
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex);
            throw RUNTIME_ERROR("Server failure");
        }
    }

    void runClient() {
        clntSock = srvrAddr.getInetAddr().socket(SOCK_STREAM);

        try {
            struct sockaddr_storage storage;
            if (::connect(clntSock, srvrAddr.get_sockaddr(storage), sizeof(storage)))
                throw SYSTEM_ERROR("connect() failure");

            for (;;) {
                unsigned char byte = 0xbd;

                set(CLIENT_WRITING);
                waitUntilSet(CLIENT_WRITE);
                if (::write(clntSock, &byte, sizeof(byte)) != 1)
                    throw SYSTEM_ERROR("write() failure");

                set(CLIENT_READING);
                waitUntilSet(CLIENT_READ);
                const auto nbytes = ::read(clntSock, &byte, sizeof(byte));
                if (nbytes == 0) {
                    LOG_NOTE("Read EOF on socket %d", clntSock);
                    break;
                }
                if (nbytes == -1)
                    throw SYSTEM_ERROR("read() failure");
            }
        } // `clntSock` allocated
        catch (const std::exception& ex) {
            ::close(clntSock);
            LOG_ERROR(ex, "Client failure");
        }
        ::close(clntSock);
    }

    void startClient() {
        clntThread = std::thread{&SocketTest::runClient, this};
    }
};

#if 0
// Tests closing the server's socket while accepting. HANGS!
TEST_F(SocketTest, CloseAtAccept)
{
    startServer();

    waitUntilSet(ACCEPTING);
    ::close(lstnSock);

    srvrThread.join();
}
#endif

// Tests shutting down the server's socket while accepting
TEST_F(SocketTest, ServerAcceptShutdown)
{
    LOG_NOTE("ServerAcceptShutdown:");
    startServer();

    waitUntilSet(ACCEPTING);
    ::shutdown(lstnSock, SHUT_RDWR);

    srvrThread.join();

    ::close(lstnSock);
}

#if 0
// Tests closing the server's socket while reading. HANGS!
TEST_F(SocketTest, CloseAtRead)
{
    startServer();
    ASSERT_TRUE(srvrThread.joinable());

    clntSock = srvrAddr.socket(SOCK_STREAM);
    srvrAddr.connect(clntSock);

    waitUntilSet(SERVER_READING);
    ::close(srvrSock);

    srvrThread.join();

    ::close(clntSock);
    ::close(lstnSock);
}
#endif

#if 0
// Tests shutting down the client's socket while writing. Hangs!
TEST_F(SocketTest, ClientWriteShutdown)
{
    LOG_NOTE("ClientWriteShutdown:");

    startServer();
    ASSERT_TRUE(srvrThread.joinable());
    startClient();
    ASSERT_TRUE(clntThread.joinable());

    waitUntilSet(CLIENT_WRITING);
    waitUntilSet(SERVER_READING);
    ::shutdown(clntSock, SHUT_RDWR);
    set(CLIENT_WRITE); // Hangs less if this before SERVER_READ
    set(SERVER_READ);

    srvrThread.join();
    clntThread.join();

    ::close(lstnSock);
}
#endif

#if 0
// Tests shutting down the server's socket while reading. Hangs!
TEST_F(SocketTest, ServerReadShutdown)
{
    LOG_NOTE("ServerReadShutdown:");

    startServer();
    ASSERT_TRUE(srvrThread.joinable());
    startClient();
    ASSERT_TRUE(clntThread.joinable());

    waitUntilSet(CLIENT_WRITING);
    waitUntilSet(SERVER_READING);
    ::shutdown(srvrSock, SHUT_RDWR);
    set(SERVER_READ);
    set(CLIENT_WRITE);

    clntThread.join();
    srvrThread.join();

    ::close(lstnSock);
}
#endif

#if 0
// Tests closing the server's socket while writing. HANGS!
TEST_F(SocketTest, CloseAtWrite)
{
    startServer();
    ASSERT_TRUE(srvrThread.joinable());

    clntSock = srvrAddr.socket(SOCK_STREAM);
    srvrAddr.connect(clntSock);
    ::write(clntSock, &clntSock, sizeof(clntSock));

    waitUntilSet(SERVER_WRITING);
    ::close(srvrSock);

    srvrThread.join();

    ::close(clntSock);
    ::close(lstnSock);
}
#endif

#if 0
// Tests shutting down the server's socket while writing. Hangs!
TEST_F(SocketTest, ServerWriteShutdown)
{
    LOG_NOTE("ServerWriteShutdown:");

    startServer();
    ASSERT_TRUE(srvrThread.joinable());
    startClient();
    ASSERT_TRUE(clntThread.joinable());

    set(CLIENT_WRITE);
    set(SERVER_READ);

    waitUntilSet(SERVER_WRITING);
    ::shutdown(srvrSock, SHUT_RDWR);
    set(SERVER_WRITE);
    set(CLIENT_READ);

    srvrThread.join();
    clntThread.join();

    ::close(lstnSock);
}
#endif

#if 0
// Tests shutting down the client's socket while it's reading. HANGS!
TEST_F(SocketTest, ClientReadShutdown)
{
    LOG_NOTE("ClientReadShutdown:");

    startServer();
    ASSERT_TRUE(srvrThread.joinable());
    startClient();
    ASSERT_TRUE(clntThread.joinable());

    set(SERVER_READ);
    set(CLIENT_WRITE);

    waitUntilSet(CLIENT_READING);
    ::shutdown(clntSock, SHUT_RDWR);
    set(CLIENT_READ);
    set(SERVER_WRITE);

    clntThread.join();
    srvrThread.join();

    ::close(lstnSock);
}
#endif

#if 0
// Tests closing the client's socket while the server's reading. Hangs!
TEST_F(SocketTest, CloseClientSocket)
{
    startServer();
    ASSERT_TRUE(srvrThread.joinable());
    startClient();
    ASSERT_TRUE(clntThread.joinable());

    waitUntilSet(SERVER_READING);
    waitUntilSet(CLIENT_WRITING);
    ::close(clntSock);
    set(SERVER_READ);
    set(CLIENT_WRITE);

    clntThread.join();
    srvrThread.join();

    ::close(lstnSock);
}
#endif

#if 0
// Tests canceling the server thread while reading
TEST_F(SocketTest, CancelServerReading)
{
    startServer();

    clntSock(srvrAddr);
    clntSock.write(true);

    waitUntilSet(SERVER_READING);
    ::pthread_cancel(srvrThread.native_handle());
    srvrThread.join();
}
#endif

#if 0
// Tests round-trip scalar exchange
TEST_F(SocketTest, ScalarExchange)
{
    startServer();

    clntSock(srvrAddr);
    int                 writeInt = 0xff00;
    int                 readInt = ~writeInt;

    clntSock.write(&writeInt, sizeof(writeInt));
    clntSock.read(&readInt, sizeof(readInt));

    EXPECT_EQ(writeInt, readInt);

    ::pthread_cancel(srvrThread.native_handle());
    srvrThread.join();
}

// Tests round-trip I/O-vector exchange
TEST_F(SocketTest, VectorExchange)
{
    startServer();

    clntSock(srvrAddr);
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
#endif

#if 0
// Tests attaching a server to a client-side socket.
// NOT POSSIBLE. listen() and connect() can't be called on the same socket
TEST_F(SocketTest, AttachServerToClientSocket)
{
    startServer();

    try {
        clntSock = srvrAddr.getInetAddr().socket(SOCK_STREAM);

        try {
            struct sockaddr_storage storage = {};
            socklen_t               socklen = sizeof(storage);

            if (::getsockname(clntSock, reinterpret_cast<struct sockaddr*>(&storage), &socklen))
                throw SYSTEM_ERROR("getsockname() failure on client-side socket " +
                        std::to_string(clntSock));

            if (::bind(clntSock, reinterpret_cast<sockaddr*>(&storage), sizeof(storage)))
                throw SYSTEM_ERROR("bind() failure on clien-side socket ");

            if (::listen(clntSock, 0))
                throw SYSTEM_ERROR("listen() failure on client-side socket");

            if (::connect(clntSock, srvrAddr.get_sockaddr(storage), sizeof(storage)))
                throw SYSTEM_ERROR("connect() failure on client-side socket");

#if 0
            uint8_t writeByte = 0xf0;
            uint8_t readByte;

            set(SERVER_READ);
            ::write(clntSock, &writeByte, sizeof(writeByte));
            set(SERVER_WRITE);
            ::read(clntSock, &readByte, sizeof(readByte));
            EXPECT_EQ(writeByte, readByte);

            struct sockaddr_in rmtAddr;
            socklen_t          addrLen = sizeof(rmtAddr);
            int sd = ::accept(clntSock, reinterpret_cast<struct sockaddr*>(&rmtAddr), &addrLen);

            if (sd == -1)
                throw SYSTEM_ERROR("accept() failure on client-side socket");

            try {

            }
            catch (const std::exception& ex) {
                ::close(sd);
                throw;
            }
#endif
            ::close(clntSock);
            clntSock = -1;
        }
        catch (const std::exception& ex) {
            ::close(clntSock);
            clntSock = -1;
            throw;
        }

        ::pthread_cancel(srvrThread.native_handle());
        ::close(lstnSock);
        lstnSock = -1;
        srvrThread.join();
    }
    catch (const std::exception& ex) {
        ::pthread_cancel(srvrThread.native_handle());
        ::close(lstnSock);
        lstnSock = -1;
        srvrThread.join();
        throw;
    }
}
#endif

}  // namespace

using namespace bicast;

int main(int argc, char **argv) {
  /*
   * Ignore SIGPIPE so that writing to a closed socket doesn't terminate the
   * process (the return-value from write() is always checked).
   */
  struct sigaction sigact;
  sigact.sa_handler = SIG_IGN;
  sigemptyset(&sigact.sa_mask);
  sigact.sa_flags = 0;
  (void)sigaction(SIGPIPE, &sigact, NULL);

  log_setName(::basename(argv[0]));
  //LOG_ERROR

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
