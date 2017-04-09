/**
 * This file declares the interface for sending notices.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Notifier.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_COMMS_NOTIFIER_H_
#define MAIN_COMMS_NOTIFIER_H_

#include "ChunkInfo.h"
#include "ProdInfo.h"

namespace hycast {

class Notifier
{
public:
    virtual ~Notifier()
    {}
    /**
     * Sends information about a product.
     * @param[in] prodInfo        Product information
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    virtual void sendNotice(const ProdInfo& prodInfo) =0;
    /**
     * Sends information about a chunk-of-data.
     * @param[in] chunkInfo       Chunk information
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    virtual void sendNotice(const ChunkInfo& chunkInfo) =0;
};

} // namespace

#endif /* MAIN_COMMS_NOTIFIER_H_ */
