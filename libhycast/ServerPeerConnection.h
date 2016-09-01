/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYRIGHT in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerConnectionServer.h
 * @author: Steven R. Emmerson
 *
 * This file declares a peer-connection that's created by the server.
 */

#ifndef SERVERPEERCONNECTION_H_
#define SERVERPEERCONNECTION_H_

#include "PeerConnection.h"
#include "Socket.h"

namespace hycast {

class ServerPeerConnection final : public PeerConnection {
public:
    /**
     * @throws std::invalid_argument if this instance already has the socket
     * @throws std::length_error if this instance is already complete
     */
    bool add_socket(const Socket& socket);
    int getNumSockets() const {return num_sockets;}
};

} // namespace

#endif /* SERVERPEERCONNECTION_H_ */
