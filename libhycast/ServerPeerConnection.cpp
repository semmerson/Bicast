/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYRIGHT in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerConnectionServer.cpp
 * @author: Steven R. Emmerson
 *
 * This file defines a peer-connection that's created by a server.
 */


#include <ServerPeerConnection.h>
#include <stdexcept>

namespace hycast {

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
bool ServerPeerConnection::add_socket(const Socket& socket)
{
    if (num_sockets == max_sockets)
        throw std::length_error("Already have " + std::to_string(max_sockets) +
                " sockets");
    for (int i = 0; i < num_sockets; i++) {
        if (socket == sockets[i])
            throw std::invalid_argument("sockets[" + std::to_string(i) +
                    "] is already socket " + socket.to_string());
    }
    sockets[num_sockets++] = socket;
    return num_sockets == max_sockets;
}

}  // namespace
