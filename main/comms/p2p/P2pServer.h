/**
 * This file declares the interface for a higher-level component that serves
 * the common aspects of a peer-to-peer network.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: P2pServer.h
 * @author: Steven R. Emmerson
 */

#ifndef P2PSERVER_H_
#define P2PSERVER_H_

#include "Chunk.h"
#include "ProdIndex.h"
#include "ProdInfo.h"
#include "RecvStatus.h"

#include <memory>

namespace hycast {

class P2pServer
{
public:
    virtual ~P2pServer() =default;
    /**
     * Indicates whether or not information on a product should be requested.
     * @param[in] prodIndex  Index of relevant product
     */
    virtual bool shouldRequest(const ProdIndex& prodIndex) =0;
    /**
     * Indicates whether or not a chunk-of-data should be requested.
     * @param[in]     chunkId  Identifier of relevant data-chunk
     */
    virtual bool shouldRequest(const ChunkId& chunkId) =0;
    /**
     * Returns information about a product.
     * @param[in]  index     Index of relevant product
     * @param[out] prodInfo  Product information
     * @retval `true`        Information exists. `prodInfo` is set.
     * @retval `false`       Information doesn't exist. `prodInfo` is not set.
     */
    virtual bool get(const ProdIndex& prodIndex, ProdInfo& prodInfo) =0;
    /**
     * Returns a chunk-of-data.
     * @param[in]  chunkId  Identifier of relevant chunk
     * @param[out] chunk    Data-chunk
     * @retval `true`       Chunk exists. `chunk` is set.
     * @retval `false`      Chunk doesn't exist. `chunk` is not set.
     */
    virtual bool get(const ChunkId& chunkId, ActualChunk& chunk) =0;
};

} // namespace

#endif /* P2PSERVER_H_ */
