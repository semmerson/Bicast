/**
 * This file implements an I/O channel.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Channel.cpp
 * @author: Steven R. Emmerson
 */

#include "../comms/Channel.h"

#include <memory>
#include "../comms/ChannelImpl.h"

namespace hycast {

SctpSock& Channel::getSocket() const
{
    return getPimpl()->getSocket();
}

unsigned Channel::getStreamId() const
{
    return getPimpl()->getStreamId();
}

size_t Channel::getSize() const
{
    return getPimpl()->getSize();
}

} // namespace
