/**
 * This file defines a connection between Hycast peers. Such a connection
 * comprises three sockets: one for notices, one for requests, and one for data.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerConnection.cpp
 * @author: Steven R. Emmerson
 */

#include "PeerConnection.h"

#include <string>

namespace hycast {

PeerConnection::PeerConnection(std::vector<Socket>& socks)
{
    if (socks.size() != max_sockets)
        throw std::invalid_argument("Expected " +
                std::to_string(max_sockets) + " sockets; got " +
                std::to_string(socks.size()));
    for (int i = 0; i < max_sockets; ++i)
        sockets[i] = socks[i];
}

}
