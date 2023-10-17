/**
 * This file tests using the Bicast C++ API to connect to a server
 *
 *  @file:  Connect_test.cpp
 * @author: Steven R. Emmerson <emmerson@ucar.edu>
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

#include "Socket.h"

#include <cassert>
#include <pthread.h>
#include <thread>

using namespace bicast;

static pthread_barrier_t barrier;
static SockAddr          srvrAddr;
static const int         NUM_SOCKS = 3;

static void startSrvr()
{
    TcpSrvrSock lstnSock{srvrAddr, 24};

    int status = ::pthread_barrier_wait(&barrier);
    assert(status == 0 || status == PTHREAD_BARRIER_SERIAL_THREAD);

    TcpSock srvrSocks[NUM_SOCKS];
    for (int i = 0; i < NUM_SOCKS; ++i)
        srvrSocks[i] = lstnSock.accept();
}

static void startClnt()
{
    int status = pthread_barrier_wait(&barrier);
    assert(status == 0 || status == PTHREAD_BARRIER_SERIAL_THREAD);

    TcpSock clntSocks[NUM_SOCKS];
    for (int i = 0; i < NUM_SOCKS; ++i)
        clntSocks[i] = TcpClntSock(srvrAddr);
}

int main(int argc, char** argv)
{
    srvrAddr = SockAddr{htonl(INADDR_LOOPBACK), 38800};

    int status = pthread_barrier_init(&barrier, NULL, 2);
    assert(status == 0);

    std::thread srvrThread{&startSrvr};
    std::thread clntThread{&startClnt};

    clntThread.join();
    srvrThread.join();

    status = pthread_barrier_destroy(&barrier);
    assert(status == 0);
}
