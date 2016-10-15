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

#ifndef PEERCONNECTIONIMPL_H_
#define PEERCONNECTIONIMPL_H_

#include "ProdInfo.h"
#include "Socket.h"

namespace hycast {

class PeerConnectionImpl final {
    typedef enum {
        PROD_INFO_STREAM_ID = 0,
        CHUNK_INFO_STREAM_ID,
        PROD_INFO_REQ_STREAM_ID,
        CHUNK_REQ_STREAM_ID,
        CHUNK_STREAM_ID,
        NUM_STREAM_IDS
    } SctpStreamId;
    Socket           sock;
public:
    /**
     * Constructs from a socket.
     * @param[in] sock  Socket
     */
    explicit PeerConnectionImpl(Socket& sock);
    /**
     * Sends information about a product to the remote peer.
     * @param[in] prodInfo  Product information
     */
    void sendProdInfo(const ProdInfo& prodInfo);
};

} // namespace

#endif /* PEERCONNECTIONIMPL_H_ */
