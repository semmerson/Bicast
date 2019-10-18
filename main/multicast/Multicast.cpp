/**
 * For sending and receiving multicast data-chunks.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Multicast.cpp
 *  Created on: Oct 15, 2019
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "error.h"
#include "Multicast.h"
#include "Socket.h"

namespace hycast {

/******************************************************************************/

class McastSndr::Impl {
    UdpSndrSock sock;

public:
    Impl(const SockAddr& grpAddr)
        : sock(grpAddr)
    {}

    SockAddr getLclAddr() const
    {
        return sock.getLclAddr();
    }

    void send(const MemChunk& chunk)
    {
        chunk.write(sock);
    }
};

McastSndr::McastSndr(const SockAddr& grpAddr)
    : pImpl{new Impl(grpAddr)}
{}


SockAddr McastSndr::getLclAddr() const
{
    return pImpl->getLclAddr();
}

void McastSndr::send(const MemChunk& chunk)
{
    pImpl->send(chunk);
}

/******************************************************************************/

class McastRcvr::Impl {
    UdpRcvrSock sock;

public:
    Impl(   const SockAddr& grpAddr,
            const InetAddr& srcAddr)
        : sock(grpAddr, srcAddr)
    {}

    void recv(UdpChunk& chunk)
    {
        chunk = UdpChunk(sock);
    }
};

/******************************************************************************/

McastRcvr::McastRcvr(
        const SockAddr& grpAddr,
        const InetAddr& srcAddr)
    : pImpl{new Impl(grpAddr, srcAddr)}
{}

void McastRcvr::recv(UdpChunk& chunk)
{
    pImpl->recv(chunk);
}

} // namespace
