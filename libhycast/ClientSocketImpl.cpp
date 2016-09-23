/**
 * This file defines the implementation of the client-side SCTP socket.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ClientSocketImpl.cpp
 * @author: Steven R. Emmerson
 */

#include "ClientSocketImpl.h"

#include <errno.h>
#include <system_error>

namespace hycast {

ClientSocketImpl::ClientSocketImpl(
        const InetSockAddr& addr,
        const uint16_t      numStreams)
    : SocketImpl(socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP), numStreams)
{
    if (sock == -1)
        throw std::system_error(errno, std::system_category(),
                "socket() failure");
    addr.connect(sock);
}

}
