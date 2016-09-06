/**
 * This file declares a connection between peers.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerConnection.h
 * @author: Steven R. Emmerson
 */

#ifndef PEERCONNECTION_H_
#define PEERCONNECTION_H_

#include "Socket.h"

#include <vector>

namespace hycast {

class PeerConnection final {
friend class PendingPeerConnections;
    static const int max_sockets = 3;
    Socket           sockets[max_sockets];
public:
    /**
     * Constructs from a vector of sockets.
     * @param[in] socks  Vector of sockets
     * @throws std::invalid_argument if `socks.size() != max_sockets`
     * @exceptionsafety Strong
     */
    PeerConnection(std::vector<Socket>& socks);
};

}

#endif /* PEERCONNECTION_H_ */
