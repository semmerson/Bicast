/**
 * For sending and receiving multicast data-chunks.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Multicast.h
 *  Created on: Oct 15, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_MULTICAST_MULTICAST_H_
#define MAIN_MULTICAST_MULTICAST_H_

#include "Chunk.h"
#include "SockAddr.h"

#include <memory>

namespace hycast {

class McastSndr
{
protected:
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    McastSndr(Impl* const impl);

public:
    /**
     * Constructs.
     *
     * @param[in] grpAddr  Address of multicast group
     */
    McastSndr(const SockAddr& grpAddr);

    /**
     * Returns the local socket address.
     *
     * @return local socket address
     */
    SockAddr getLclAddr() const;

    /**
     * Multicasts a chunk.
     */
    void send(const MemChunk& chunk);
};

class McastRcvr
{
protected:
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    McastRcvr(Impl* const impl);

public:
    /**
     * Constructs.
     *
     * @param[in] grpAddr  Address of multicast group
     * @param[in] srcAddr  Internet address of sending host
     */
    McastRcvr(
            const SockAddr& grpAddr,
            const InetAddr& srcAddr);

    /**
     * Receives a multicast chunk.
     */
    void recv(UdpChunk& chunk);
};

} // namespace

#endif /* MAIN_MULTICAST_MULTICAST_H_ */
