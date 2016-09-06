/**
 * This file defines the implementation of a peer.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerImpl.cpp
 * @author: Steven R. Emmerson
 */

#include "PeerImpl.h"

namespace hycast {

PeerImpl::PeerImpl(PeerConnection& conn) noexcept
    : conn(conn)
{
}

} // namespace
