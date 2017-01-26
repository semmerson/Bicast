/**
 * This file implements a handle class for a receiver of multicast objects.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: McastRcvr.cpp
 * @author: Steven R. Emmerson
 */

#include <McastRcvr.h>

namespace hycast {

/**
 * Implementation of a receiver of objects serialized in UDP packets.
 */
class McastRcvr::Impl
{
    McastUdpSock   mcastSock; // Multicast socket from which to read objects
    const unsigned version;   // Protocol version

public:
    Impl(   McastUdpSock&  mcastSock,
            const unsigned version)
        : mcastSock{mcastSock}
        , version{version}
    {}
};

McastRcvr::McastRcvr(
        McastUdpSock&  mcastSock,
        const unsigned version)
    : pImpl{new McastRcvr::Impl(mcastSock, version)}
{}

} // namespace
