/**
 * This file declares a thread-safe performance meter for measuring data-product
 * transmission.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: PerfMeter.h
 *  Created on: Aug 22, 2017
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_COMMS_PERFMETER_H_
#define MAIN_COMMS_PERFMETER_H_

#include "ProdInfo.h"

#include <memory>
#include <ostream>
#include <string>

namespace hycast {

class PerfMeter
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    PerfMeter();

    /**
     * @param[in] prodInfo  Product information
     * @threadsafety        Safe
     */
    void product(const ProdInfo& prodInfo) const;

    /**
     * Returns the number of products.
     */
    unsigned long getProdCount() const;

    /**
     * @threadsafety        Safe
     */
    void stop() const;

    /**
     * @threadsafety        Safe
     */
    friend std::ostream& operator<<(
            std::ostream&    ostream,
            const PerfMeter& perfMeter);
};

} // namespace

#endif /* MAIN_COMMS_PERFMETER_H_ */
