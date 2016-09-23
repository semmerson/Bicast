/**
 * This file defines an std::streambuf for an SCTP socket.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: SctpBuf.cpp
 * @author: Steven R. Emmerson
 */

#include "SctpBuf.h"

namespace hycast {

SctpBuf::SctpBuf(
        Socket&        sock,
        const unsigned streamId)
    : sock(sock),
      streamId(streamId)
{
}

} // namespace
