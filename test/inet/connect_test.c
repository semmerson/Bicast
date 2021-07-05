/**
 * This file tests `connect(2)`
 *
 *  @file:  
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

#undef NDEBUG

#include <assert.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stddef.h>
#include <sys/socket.h>
#include <unistd.h>

#define SRVR_PORT 38800
#define NUM_SOCKS 3

static pthread_barrier_t  barrier;
static struct sockaddr_in srvrAddr;

static void* startSrvr(void* arg)
{
    int lstnSock = socket(AF_INET, SOCK_STREAM, 0);
    assert(lstnSock >= 0);

    const int enable = 1;
    int       status = setsockopt(lstnSock, SOL_SOCKET, SO_REUSEADDR, &enable,
            sizeof(enable));
    assert(status == 0);

    status = bind(lstnSock, (const struct sockaddr*)&srvrAddr, sizeof(srvrAddr));
    assert(status == 0);

    status = listen(lstnSock, 24);
    assert(status == 0);

    status = pthread_barrier_wait(&barrier);
    assert(status == 0 || status == PTHREAD_BARRIER_SERIAL_THREAD);

    int srvrSocks[NUM_SOCKS];
    for (int i = 0; i < NUM_SOCKS; ++i) {
        srvrSocks[i] = accept(lstnSock, NULL, NULL);
        assert(srvrSocks[i] >= 0);
    }

    for (int i = 0; i < NUM_SOCKS; ++i) {
        status = close(srvrSocks[i]);
        assert(status == 0);
    }

    status = close(lstnSock);
    assert(status == 0);

    return NULL;
}

static void* startClnt(void* arg)
{
    int status = pthread_barrier_wait(&barrier);
    assert(status == 0 || status == PTHREAD_BARRIER_SERIAL_THREAD);

    int clntSocks[NUM_SOCKS];

    for (int i = 0; i < NUM_SOCKS; ++i) {
        clntSocks[i] = socket(AF_INET, SOCK_STREAM, 0);
        assert(clntSocks[i] >= 0);
    }

    for (int i = 0; i < NUM_SOCKS; ++i) {
        status = connect(clntSocks[i], (const struct sockaddr*)&srvrAddr,
                sizeof(srvrAddr));
        assert(status == 0);
    }

    for (int i = 0; i < NUM_SOCKS; ++i) {
        status = close(clntSocks[i]);
        assert(status == 0);
    }

    return NULL;
}

int main(int argc, char** argv)
{
    int status = pthread_barrier_init(&barrier, NULL, 2);
    assert(status == 0);

    srvrAddr.sin_family      = AF_INET;
    srvrAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    srvrAddr.sin_port        = htons(SRVR_PORT);

    pthread_t srvrThread;
    status = pthread_create(&srvrThread, NULL, startSrvr, NULL);
    assert(status == 0);

    pthread_t clntThread;
    status = pthread_create(&clntThread, NULL, startClnt, NULL);
    assert(status == 0);

    status = pthread_join(clntThread, NULL);
    assert(status == 0);

    status = pthread_join(srvrThread, NULL);
    assert(status == 0);

    status = pthread_barrier_destroy(&barrier);
    assert(status == 0);
}
