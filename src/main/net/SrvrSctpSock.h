/**
 * This file declares a server-side SCTP socket.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ClientSocket.h
 * @author: Steven R. Emmerson
 */

#ifndef SERVERSOCKET_H_
#define SERVERSOCKET_H_

#include "InetSockAddr.h"
#include "SctpSock.h"

namespace hycast {

class SrvrSctpSock final : public SctpSock {
public:
    /**
     * Constructs from an Internet socket address and the number of SCTP
     * streams.
     * @param[in] addr        Internet socket address
     * @param[in] numStreams  Number of SCTP streams
     */
    SrvrSctpSock(
            const InetSockAddr& addr,
            const uint16_t      numStreams);
    /**
     * Accepts an incoming connection on the socket.
     * @return The accepted connection
     * @exceptionsafety Basic
     * @threadsafety    Unsafe but compatible
     */
    SctpSock accept() const;
};

} // namespace

#endif /* SERVERSOCKET_H_ */
