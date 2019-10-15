/**
 * An in-memory data-product.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: MemProd.h
 *  Created on: Sep 27, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_STORAGE_MEMPROD_H_
#define MAIN_STORAGE_MEMPROD_H_

#include <main/protocol/Chunk.h>
#include <memory>

namespace hycast {

class MemProd
{
protected:
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    MemProd(Impl* const impl);

public:
    /**
     * Default constructs.
     */
    MemProd() =default;

    /**
     * Accepts a data-chunk for incorporation.
     *
     * @param[in] chunk    Data-chunk
     * @retval    `true`   Chunk was accepted
     * @retval    `false`  Chunk was not accepted. `log()` was called.
     * @threadsafety       Safe
     * @exceptionsafety    Strong guarantee
     * @cancellationpoint  No
     */
    bool accept(TcpChunk chunk) const;
};

} // namespace

#endif /* MAIN_STORAGE_MEMPROD_H_ */
