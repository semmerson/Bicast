/**
 * This file implements a multicast sender of data-products.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: McastSender.cpp
 * @author: Steven R. Emmerson
 */

#include "config.h"

#include "UdpSock.h"
#include "McastSender.h"

namespace hycast {

class McastSender::Impl final
{
    OutUdpSock sock;

public:
    /**
     * Constructs.
     * @param[in] mcastAddr  Socket address of the multicast group
     * @param[in] version    Protocol version
     * @throws std::system_error  `socket()` failure
     */
    Impl(   const InetSockAddr& mcastAddr,
            const unsigned      version)
        : sock(mcastAddr)
    {}
};

McastSender::McastSender(
        const InetSockAddr& mcastAddr,
        const unsigned      version)
    : pImpl{new Impl(mcastAddr, version)}
{}

void McastSender::send(Product& prod)
{
}

} // namespace
