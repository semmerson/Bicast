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
#include "ClientSocketImpl.h"

namespace hycast {

ClientSocket::ClientSocket(
        const InetSockAddr& addr,
        const uint16_t      numStreams)
    : Socket(new ClientSocketImpl(addr, numStreams))
{
}

} // namespace
