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

#include <comms/ChannelImpl.h>

namespace hycast {

void ChannelImpl::SctpStream::send(
        const struct iovec* const iovec,
        const int                 iovcnt);

size_t ChannelImpl::SctpStream::recv(
        const struct iovec* iovec,
        const int           iovcnt,
        const bool          peek = false);

} // namespace
