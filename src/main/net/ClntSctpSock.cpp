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

#include "ClntSctpSock.h"

#include "InetSockAddr.h"

#include <errno.h>
#include <system_error>
#include "SctpSockImpl.h"

namespace hycast {

class ClntSctpSockImpl final : public SctpSockImpl
{
public:
    ClntSctpSockImpl(
            const InetSockAddr& addr,
            const unsigned      numStreams)
        : SctpSockImpl(socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP), numStreams)
    {
        addr.connect(sock.load());
    }
};

ClntSctpSock::ClntSctpSock(
        const InetSockAddr& addr,
        const unsigned      numStreams)
    : SctpSock(new ClntSctpSockImpl(addr, numStreams))
{}

} // namespace
