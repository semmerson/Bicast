/**
 * TCP transport for sending and receiving multicast data-chunks.
 *
 *        File: TcpXprt.h
 *  Created on: Oct 15, 2019
 *      Author: Steven R. Emmerson
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

#ifndef MAIN_PROTOCOL_TCP_XPRT_H_
#define MAIN_PROTOCOL_TCP_XPRT_H_

#include <hycast.h>
#include <main/inet/SockAddr.h>
#include <memory>

namespace hycast {

class TcpChunk;

class TcpXprt
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Constructs a TCP transport.
     *
     * @param[in] sock  TCP socket
     */
    TcpXprt(TcpSock& sock);

    void send(const InfoId& chunkId) const;

    void send(const SegId& chunkId) const;

    void send(const MemInfoChunk& chunk) const;

    void send(const MemSeg& chunk) const;

    /**
     * Receives a chunk identifier.
     */
    ChunkId recv();

    /**
     * Receives a chunk.
     */
    TcpChunk recv();
};

} // namespace

#endif /* MAIN_PROTOCOL_TCP_XPRT_H_ */
