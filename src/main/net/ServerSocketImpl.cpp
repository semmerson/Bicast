/**
 * This file defines the implementation of the server-side SCTP socket.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ServerSocketImpl.cpp
 * @author: Steven R. Emmerson
 */

#include "ServerSocketImpl.h"

#include <errno.h>
#include <system_error>

namespace hycast {

ServerSocketImpl::ServerSocketImpl(
        const InetSockAddr& addr,
        const uint16_t      numStreams)
    : SocketImpl(socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP), numStreams)
{
    if (sock == -1)
        throw std::system_error(errno, std::system_category(),
                "socket() failure");
    addr.bind(sock);
    if (listen(sock, 5))
        throw std::system_error(errno, std::system_category(),
                "listen() failure: sock=" + std::to_string(sock) +
                ", addr=" + to_string());
}

std::shared_ptr<SocketImpl> ServerSocketImpl::accept()
{
    socklen_t len = 0;
    int sd = ::accept(sock, (struct sockaddr*)nullptr, &len);
    if (sd < 0)
        throw std::system_error(errno, std::system_category(),
                "accept() failure: sock=" + std::to_string(sock));
    return std::shared_ptr<SocketImpl>(new SocketImpl(sd, getNumStreams()));
}

} // namespace
