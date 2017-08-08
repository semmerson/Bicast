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
#include "config.h"

#include "ClntSctpSock.h"
#include "error.h"

#include <unistd.h>

namespace hycast {

ClntSctpSock::ClntSctpSock(
        const InetSockAddr& addr,
        const unsigned      numStreams)
    : SctpSock(numStreams)
{
    auto sd = createSocket();
    try {
        addr.connect(sd);
        setSock(sd);
    }
    catch (const std::exception& e) {
        (void)::close(sd);
        std::throw_with_nested(RuntimeError(__FILE__, __LINE__,
                "Couldn't connect SCTP socket to remote endpoint"));
    }
}

} // namespace
