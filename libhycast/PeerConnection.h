/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerConnection.h
 * @author: Steven R. Emmerson
 *
 * This file declares a connection between peers.
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
            PeerConnection();
    virtual ~PeerConnection() = 0; // "= 0" => Abstract base class
};

}

#endif /* PEERCONNECTION_H_ */
