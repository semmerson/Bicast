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

#include "InetSockAddr.h"
#include "ServerSocket.h"
#include "SocketImpl.h"

#include <errno.h>
#include <memory>
#include <system_error>

namespace hycast {

class ServerSocketImpl final : public SocketImpl
{
public:
    ServerSocketImpl(
            const InetSockAddr& addr,
            const uint16_t      numStreams)
        : SocketImpl(socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP), numStreams)
    {
        int sd = sock.load();
        if (sd == -1)
            throw std::system_error(errno, std::system_category(),
                    "socket() failure");
        addr.bind(sd);
        if (listen(sd, 5))
            throw std::system_error(errno, std::system_category(),
                    "listen() failure: sock=" + std::to_string(sd) +
                    ", addr=" + to_string());
    }

    /**
     * Accepts an incoming connection on the socket.
     * @return The accepted connection
     * @exceptionsafety Basic
     * @threadsafety    Unsafe but compatible
     */
    std::shared_ptr<SocketImpl> accept()
    {
        socklen_t len = 0;
        int sck = sock.load();
        int sd = ::accept(sck, (struct sockaddr*)nullptr, &len);
        if (sd < 0)
            throw std::system_error(errno, std::system_category(),
                    "accept() failure: sock=" + std::to_string(sck));
        return std::shared_ptr<SocketImpl>(new SocketImpl(sd, getNumStreams()));
    }
};

ServerSocket::ServerSocket(
        const InetSockAddr& addr,
        const uint16_t      numStreams)
    : Socket(new ServerSocketImpl(addr, numStreams))
{}

Socket ServerSocket::accept() const
{
    return Socket((static_cast<ServerSocketImpl*>(pImpl.get()))->accept());
}

} // namespace
