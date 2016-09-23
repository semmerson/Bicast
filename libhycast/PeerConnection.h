/**
 * This file declares a connection between peers.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerConnection.h
 * @author: Steven R. Emmerson
 */

#ifndef PEERCONNECTION_H_
#define PEERCONNECTION_H_

#include "ProdInfo.h"
#include "Socket.h"

namespace hycast {

class PeerConnection final {
    Socket           sock;
public:
    typedef enum {
        PROD_INFO_STREAM_ID = 0,
        CHUNK_INFO_STREAM_ID,
        PROD_INFO_REQ_STREAM_ID,
        CHUNK_REQ_STREAM_ID,
        CHUNK_STREAM_ID,
        NUM_STREAM_IDS
    } SctpStreamId;
    /**
     * Constructs from a socket.
     * @param[in] sock  Socket
     * @exceptionsafety Strong
     */
    PeerConnection(Socket& sock);
    /**
     * Sends information about a product to the remote peer.
     */
    void sendProdInfo(const ProdInfo& prodInfo);
};

}

#endif /* PEERCONNECTION_H_ */
