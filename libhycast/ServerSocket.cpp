/**
 * This file defines a server-side socket.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ServerSocket.cpp
 * @author: Steven R. Emmerson
 */

#include "ServerSocket.h"
#include "ServerSocketImpl.h"

#include <memory>

namespace hycast {

ServerSocket::ServerSocket(
        const InetSockAddr& addr,
        const uint16_t      numStreams)
    : Socket(new ServerSocketImpl(addr, numStreams))
{
}

Socket ServerSocket::accept()
{
    return Socket{(static_cast<ServerSocketImpl*>(pImpl.get()))->accept()};
}

} // namespace
