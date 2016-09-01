/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerConnection.cpp
 * @author: Steven R. Emmerson
 *
 * This file defines a connection between Hycast peers. Such a connection
 * comprises three sockets: one for notices, one for requests, and one for data.
 */

#include "PeerConnection.h"

namespace hycast {

PeerConnection::PeerConnection()
    : num_sockets{0}
{
}

/**
 * Destroys a `PeerConnection`. The sockets will be closed if this instance
 * contains the last references to them. This definition is necessary in order
 * to make this class an abstract base class.
 */
PeerConnection::~PeerConnection()
{
}

}
