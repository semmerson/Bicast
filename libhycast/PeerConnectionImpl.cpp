/**
 * This file implements a connection between Hycast peers.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerConnectionImpl.cpp
 * @author: Steven R. Emmerson
 */

#include "PeerConnectionImpl.h"

namespace hycast {

PeerConnectionImpl::PeerConnectionImpl(Socket& sock)
    : sock(sock)
{
}

void PeerConnectionImpl::sendProdInfo(const ProdInfo& prodInfo)
{
}

#if 0
void PeerConnection::run()
{
    for (;;) {
        uint32_t size = sock.getSize();
        if (size == 0)
            break; // EOF
        switch (sock.getStreamId()) {
            case PROD_INFO_STREAM_ID:
                prodInfoQueue.add(ProdInfo(prodInfoIstream, version));
        }
    }
}
#endif

} // namespace
