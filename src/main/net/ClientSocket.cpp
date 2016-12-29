/**
 * This file defines a client-side socket.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ClientSocket.cpp
 * @author: Steven R. Emmerson
 */

#include "ClientSocket.h"
#include "InetSockAddr.h"
#include "SocketImpl.h"

#include <errno.h>
#include <system_error>

namespace hycast {

class ClientSocketImpl final : public SocketImpl
{
public:
    ClientSocketImpl(
            const InetSockAddr& addr,
            const uint16_t      numStreams)
        : SocketImpl(socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP), numStreams)
    {
        addr.connect(sock.load());
    }
};

ClientSocket::ClientSocket(
        const InetSockAddr& addr,
        const uint16_t      numStreams)
    : Socket(new ClientSocketImpl(addr, numStreams))
{}

} // namespace
