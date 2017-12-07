/**
 * This file declares the interface for a component that receives multicast
 * content.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: McastContentRcvr.h
 * @author: Steven R. Emmerson
 */

#ifndef MCASTCONTENTRCVR_H_
#define MCASTCONTENTRCVR_H_

#include "Chunk.h"
#include "ProdIndex.h"
#include "ProdInfo.h"

#include <memory>

namespace hycast {

class McastContentRcvr {
public:
    virtual ~McastContentRcvr() =default;
    /**
     * Receives information about a product.
     * @param[in] info  Product information
     */
    virtual void receive(const ProdInfo& info) =0;
    /**
     * Receives a chunk-of-data.
     * @param[in]     chunk  Chunk-of-data
     */
    virtual void receive(LatentChunk chunk) =0;
};

} // namespace

#endif /* MCASTCONTENTRCVR_H_ */
