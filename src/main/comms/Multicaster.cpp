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

#include <Multicaster.h>

namespace hycast {

/**
 * Implementation of a receiver of objects serialized in UDP packets.
 */
class Multicaster::Impl
{
    McastUdpSock   mcastSock; // Multicast socket from which to read objects
    const unsigned version;   // Protocol version
    MsgRcvr*       msgRcvr;   // Receiver of multicast objects

public:
    /**
     * Constructs.
     * @param[in] mcastSock  Multicast socket
     * @param[in] version    Protocol version
     * @param[in] msgRcvr    Receiver of multicast objects or `nullptr`. If
     *                       non-null, then must exist for the duration of the
     *                       constructed instance.
     */
    Impl(   McastUdpSock&  mcastSock,
            const unsigned version,
            MsgRcvr*       msgRcvr)
        : mcastSock{mcastSock}
        , version{version}
        , msgRcvr{msgRcvr}
    {}
};

Multicaster::Multicaster(
        McastUdpSock&  mcastSock,
        const unsigned version,
        MsgRcvr*       msgRcvr)
    : pImpl{new Multicaster::Impl(mcastSock, version, msgRcvr)}
{}

} // namespace
