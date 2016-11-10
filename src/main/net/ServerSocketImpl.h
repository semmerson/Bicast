/**
 * This file declares an implementation of a server-side SCTP socket.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ServerSocketImpl.h
 * @author: Steven R. Emmerson
 */

#ifndef SERVERSOCKETIMPL_H_
#define SERVERSOCKETIMPL_H_

#include "InetSockAddr.h"
#include "SocketImpl.h"

namespace hycast {

class ServerSocketImpl final : public SocketImpl {
public:
    ServerSocketImpl(
            const InetSockAddr& addr,
            const uint16_t      numStreams);
    /**
     * Accepts an incoming connection on the socket.
     * @return The accepted connection
     * @exceptionsafety Basic
     * @threadsafety    Unsafe but compatible
     */
    std::shared_ptr<SocketImpl> accept();
};

} // namespace

#endif /* SERVERSOCKETIMPL_H_ */
