/**
 * Interface for receiving messages from a peer.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: ReceivePeer.h
 *  Created on: May 10, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_PEER_PEERMSGRCVR_H_
#define MAIN_PEER_PEERMSGRCVR_H_

#include <main/protocol/Chunk.h>
#include <memory>

namespace hycast {

class Peer; // Forward declaration because Peer.h includes this file

class PeerMsgRcvr
{
public:
    virtual ~PeerMsgRcvr() noexcept
    {}

    /**
     * Indicates if a chunk should be requested from a peer.
     *
     * @param[in] chunkId  ID of `Chunk`
     * @param[in] rmtAddr  Socket address of remote peer
     * @retval    `true`   The chunk should be requested from the remote peer
     * @retval    `false`  The chunk should not be requested from the remote peer
     */
    virtual bool shouldRequest(
            const ChunkId&  chunkId,
            const SockAddr& rmtAddr) =0;

    /**
     * Obtains a chunk for a peer.
     *
     * @param[in] chunkId  ID of requested `Chunk`
     * @param[in] rmtAddr  Socket address of remote peer
     * @return             The chunk. Will be empty if it doesn't exist.
     */
    virtual MemChunk get(
            const ChunkId&  chunkId,
            const SockAddr& rmtAddr) =0;

    /**
     * Processes a `Chunk` from a peer.
     *
     * @param[in] chunk    The `Chunk`
     * @param[in] rmtAddr  Socket address of remote peer
     */
    virtual void hereIs(
            TcpChunk       chunk,
            const SockAddr& rmtAddr) =0;
};

} // namespace

#endif /* MAIN_PEER_PEERMSGRCVR_H_ */
