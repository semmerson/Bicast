/**
 * This file declares an implementation of a client-side SCTP socket.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ClientSocketImpl.h
 * @author: Steven R. Emmerson
 */

#ifndef CLIENTSOCKETIMPL_H_
#define CLIENTSOCKETIMPL_H_

#include "InetSockAddr.h"
#include "SocketImpl.h"

namespace hycast {

class ClientSocketImpl final : public SocketImpl {
public:
    ClientSocketImpl(
            const InetSockAddr& addr,
            const uint16_t      numStreams);
};

} // namespace

#endif /* CLIENTSOCKETIMPL_H_ */
