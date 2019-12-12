/**
 * TCP transport for sending and receiving multicast data-chunks.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: TcpXprt.h
 *  Created on: Oct 15, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_PROTOCOL_TCP_XPRT_H_
#define MAIN_PROTOCOL_TCP_XPRT_H_

#include <hycast.h>
#include "SockAddr.h"

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
