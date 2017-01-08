/**
 * This file implements an I/O channel for sending and receiving Hycast
 * messages.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: ChannelImpl.cpp
 * @author: Steven R. Emmerson
 */

#include "ChannelImpl.h"

namespace hycast {

ChannelImpl::ChannelImpl(
        SctpSock&            sock,
        const unsigned     streamId,
        const unsigned     version)
    : sock(sock),
      streamId(streamId),
      version(version)
{
}

} // namespace
