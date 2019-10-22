/**
 * Repository of products.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Repository.h
 *  Created on: Sep 27, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_REPOSITORY_REPOSITORY_H_
#define MAIN_REPOSITORY_REPOSITORY_H_

#include "PeerMsgRcvr.h"

#include <memory>
#include <string>

namespace hycast {

class Repository : public PeerMsgRcvr
{
protected:
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    Repository(Impl* const impl);

public:
    /**
     * Constructs.
     *
     * @param[in] rootPath   Pathname of the root of the repository
     * @param[in] chunkSize  Size of data-segments in bytes
     */
    Repository(
            const std::string& rootPath,
            ChunkSize          segSize);

    /**
     * Indicates if a chunk should be requested from a peer.
     *
     * @param[in] chunkId  ID of `Chunk`
     * @param[in] rmtAddr  Socket address of remote peer
     * @retval    `true`   Chunk should be requested from the remote peer
     * @retval    `false`  Chunk should not be requested from the remote peer
     */
    bool shouldRequest(
            const ChunkId&  chunkId,
            const SockAddr& rmtAddr);

    /**
     * Obtains a chunk for a remote peer.
     *
     * @param[in] chunkId  ID of requested `Chunk`
     * @param[in] rmtAddr  Socket address of remote peer
     * @return             The chunk. Will be empty if it doesn't exist.
     */
    MemChunk get(
            const ChunkId&  chunkId,
            const SockAddr& rmtAddr);

    /**
     * Accepts a multicast chunk.
     *
     * @param[in] chunk    Chunk
     * @threadsafety       Safe
     * @exceptionsafety    Strong guarantee
     * @cancellationpoint  No
     */
    void hereIs(UdpChunk chunk) const;

    /**
     * Accepts a chunk from a peer.
     *
     * @param[in] chunk    Chunk
     * @threadsafety       Safe
     * @exceptionsafety    Strong guarantee
     * @cancellationpoint  No
     */
    void hereIs(TcpChunk chunk) const;
};

} // namespace

#endif /* MAIN_REPOSITORY_REPOSITORY_H_ */
