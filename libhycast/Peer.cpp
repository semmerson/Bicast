/**
 * This file implements the virtual destructor for the pure abstract base class
 * `Peer` (to prevent an undefined reference during linking).
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Peer.cpp
 * @author: Steven R. Emmerson
 */

#include "Peer.h"

namespace hycast {

Peer::~Peer()
{
}

} // namespace
