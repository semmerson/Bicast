/**
 * This file declares a peer-connection that's created by the server.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYRIGHT in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerConnectionServer.h
 * @author: Steven R. Emmerson
 */

#ifndef SERVERPEERCONNECTION_H_
#define SERVERPEERCONNECTION_H_

#include "PeerConnection.h"
#include "Socket.h"

namespace hycast {

class ServerPeerConnection final : public PeerConnection {
public:
    /**
     * Adds a socket.
     * @param[in] socket  Socket to be added
     * @retval `false` Connection is incomplete
     * @retval `true`  Connection is complete
     * @throws std::invalid_argument if this instance already has the socket
     * @throws std::length_error if this instance is already complete
     * @throws std::bad_alloc if required memory can't be allocated
     * @exceptionsafety Strong
     */
    bool add_socket(const Socket& socket);
    /**
     * Returns the number of sockets that this instance has.
     * @return The number of sockets that this instance has
     * @exceptionsafety Nothrow
     */
    int getNumSockets() const noexcept {return num_sockets;}
};

} // namespace

#endif /* SERVERPEERCONNECTION_H_ */
