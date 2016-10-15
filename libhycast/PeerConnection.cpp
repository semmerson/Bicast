/**
 * This file implements a connection between Hycast peers.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerConnectionImpl.cpp
 * @author: Steven R. Emmerson
 */

#include "PeerConnection.h"
#include "PeerConnectionImpl.h"

namespace hycast {

PeerConnection::PeerConnection(Socket& sock)
    : pImpl(new PeerConnectionImpl(sock))
{
}

void PeerConnection::sendProdInfo(const ProdInfo& prodInfo)
{
}

} // namespace
