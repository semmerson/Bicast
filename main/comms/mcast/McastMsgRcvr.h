/**
 * This file declares the interface for an object that receives multicast
 * messages.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: McastMsgRcvr.h
 * @author: Steven R. Emmerson
 */

#ifndef MCASTMSGRCVR_H_
#define MCASTMSGRCVR_H_

#include "Chunk.h"
#include "ProdIndex.h"
#include "ProdInfo.h"

#include <memory>

namespace hycast {

class McastMsgRcvr {
public:
    virtual ~McastMsgRcvr() =default;
    /**
     * Receives a notice about a new product.
     * @param[in]     info  Information about the product
     */
    virtual void recvNotice(const ProdInfo& info) =0;
    /**
     * Receives a chunk-of-data.
     * @param[in]     chunk  Chunk-of-data
     */
    /*
     * For an unknown reason, the compiler complains if the `recvData` parameter
     * is a `LatentChunk&` and not a `LatentChunk`. This is acceptable, however,
     * because `LatentChunk` uses the pImpl idiom.
     */
    virtual void recvData(LatentChunk chunk) =0;
};

} // namespace

#endif /* MCASTMSGRCVR_H_ */
