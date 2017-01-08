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

#include "SrvrSctpSock.h"

#include "InetSockAddr.h"
#include "SctpSockImpl.h"
#include <errno.h>
#include <memory>
#include <system_error>

namespace hycast {

class SrvrSockImpl final : public SctpSockImpl
{
public:
    SrvrSockImpl(
            const InetSockAddr& addr,
            const uint16_t      numStreams)
        : SctpSockImpl(socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP), numStreams)
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
    std::shared_ptr<SctpSockImpl> accept()
    {
        socklen_t len = 0;
        int sck = sock.load();
        int sd = ::accept(sck, (struct sockaddr*)nullptr, &len);
        if (sd < 0)
            throw std::system_error(errno, std::system_category(),
                    "accept() failure: sock=" + std::to_string(sck));
        return std::shared_ptr<SctpSockImpl>(new SctpSockImpl(sd, getNumStreams()));
    }
};

SrvrSctpSock::SrvrSctpSock(
        const InetSockAddr& addr,
        const uint16_t      numStreams)
    : SctpSock(new SrvrSockImpl(addr, numStreams))
{}

SctpSock SrvrSctpSock::accept() const
{
    return SctpSock((static_cast<SrvrSockImpl*>(pImpl.get()))->accept());
}

} // namespace
