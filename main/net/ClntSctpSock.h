/**
 * This file declares a client-side SCTP socket.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ClientSocket.h
 * @author: Steven R. Emmerson
 */

#ifndef CLIENTSOCKET_H_
#define CLIENTSOCKET_H_

#include "InetSockAddr.h"
#include "SctpSock.h"

namespace hycast {

class ClntSctpSock final : public SctpSock {
public:
    /**
     * Constructs. Blocks connecting to remote server.
     * @param[in] addr         Internet socket address of remote server
     * @param[in] numStreams   Number of SCTP streams
     * @throw InvalidArgument  `numStreams < 0`
     */
    ClntSctpSock(
            const InetSockAddr& addr,
            const unsigned      numStreams);
};

} // namespace

#endif /* CLIENTSOCKET_H_ */
