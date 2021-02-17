/**
 * UDP transport for sending and receiving multicast data-chunks.
 *
 *        File: UdpXprt.h
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

#ifndef MAIN_PROTOCOL_UDP_XPRT_H_
#define MAIN_PROTOCOL_UDP_XPRT_H_

#include <hycast.h>
#include <main/inet/SockAddr.h>
#include <memory>

namespace hycast {

class UdpChunk;

class UdpXprt
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Constructs a sending UDP transport.
     *
     * @param[in] grpAddr  Address of multicast group
     */
    UdpXprt(const SockAddr& grpAddr);

    /**
     * Constructs a source-specific, receiving UDP transport.
     *
     * @param[in] grpAddr  Address of multicast group
     * @param[in] srcAddr  Internet address of sending host
     */
    UdpXprt(const SockAddr& grpAddr,
            const InetAddr& srcAddr);

    void send(const MemInfoChunk& chunk) const;

    void send(const MemSeg& chunk) const;

    /**
     * Receives a multicast chunk.
     */
    UdpChunk recv();
};

} // namespace

#endif /* MAIN_PROTOCOL_UDP_XPRT_H_ */
