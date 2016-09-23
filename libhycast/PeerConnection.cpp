/**
 * This file defines a connection between Hycast peers. Such a connection
 * comprises three sockets: one for notices, one for requests, and one for data.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerConnection.cpp
 * @author: Steven R. Emmerson
 */

#include "PeerConnection.h"

#include <netinet/sctp.h>
#include <string>

namespace hycast {

PeerConnection::PeerConnection(Socket& sock)
    : sock(sock)
      // , prodInfoOstream(sock, PROD_INFO_STREAM_ID)
{
}

void PeerConnection::sendProdInfo(const ProdInfo& prodInfo)
{
    // prodInfo.serialize(prodInfoOstream, version);
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
