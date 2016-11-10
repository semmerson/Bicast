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

#include "Channel.h"
#include <ChannelImpl.h>

#include <memory>

namespace hycast {

Socket& Channel::getSocket() const
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
