/**
 * UDP transport for sending and receiving multicast data-chunks.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: UdpXprt.h
 *  Created on: Oct 15, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_PROTOCOL_UDP_XPRT_H_
#define MAIN_PROTOCOL_UDP_XPRT_H_

#include <hycast.h>
#include "SockAddr.h"

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
