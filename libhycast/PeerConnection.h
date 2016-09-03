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

namespace hycast {

class PeerConnection {
protected:
    static const int max_sockets = 3;
    int              num_sockets;
    Socket           sockets[max_sockets];
public:
    /**
     * Constructs from nothing.
     * @exceptionsafety Nothrow
     */
    PeerConnection() noexcept;
    /**
     * Destroys a `PeerConnection`. The sockets will be closed if this instance
     * contains the last references to them. This definition is necessary in
     * order to make this class an abstract base class.
     * @exceptionsafety Nothrow
     */
    virtual ~PeerConnection() noexcept = 0; // "= 0" => Abstract base class
};

}

#endif /* PEERCONNECTION_H_ */
