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

#include "ProdInfo.h"

namespace hycast {

class Notifier
{
public:
    virtual ~Notifier()
    {}
    /**
     * Notifies about available information on a product.
     * @param[in] prodIndex       Relevant product index
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    virtual void notify(const ProdIndex& prodIndex) const =0;
    /**
     * Notifies about an available chunk-of-data.
     * @param[in] chunkId         Relevant chunk identifier
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    virtual void notify(const ChunkId& chunkId) const =0;
};

} // namespace

#endif /* MAIN_COMMS_NOTIFIER_H_ */
