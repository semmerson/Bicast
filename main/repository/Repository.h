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

#include <memory>
#include <string>

namespace hycast {

class Repository
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
     * @param[in] chunkSize  Canonical size of data-segments in bytes
     */
    Repository(
            const std::string& rootPath,
            SegSize            segSize);

    /**
     * Saves product information.
     *
     * @param[in] prodInfo  Product information
     * @threadsafety       Safe
     * @exceptionsafety    Strong guarantee
     * @cancellationpoint  No
     */
    void save(const ProdInfo& prodInfo) const;

    /**
     * Saves a socket-based data-segment.
     *
     * @param[in] sockSeg  Socket-based data-segment
     * @threadsafety       Safe
     * @exceptionsafety    Strong guarantee
     * @cancellationpoint  No
     */
    void save(const SockSeg& sockSeg) const;
};

} // namespace

#endif /* MAIN_REPOSITORY_REPOSITORY_H_ */
