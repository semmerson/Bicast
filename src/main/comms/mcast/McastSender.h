/**
 * This file declares a multicast sender of data-products.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: McastSender.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_MCAST_MCASTSENDER_H_
#define MAIN_MCAST_MCASTSENDER_H_

#include "InetSockAddr.h"
#include "Product.h"

#include <memory>

namespace hycast {

class McastSender final
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    typedef uint16_t MsgIdType;

    static const MsgIdType prodInfoId = 0;
    static const MsgIdType chunkId = 1;

    /**
     * Constructs.
     * @param[in] mcastAddr  Socket address of the multicast group
     * @param[in] version    Protocol version
     * @throws std::system_error  `socket()` failure
     */
    McastSender(
            const InetSockAddr& mcastAddr,
            const unsigned      version);

    /**
     * Sends a data-product.
     * @param[in] prod  Data-product
     */
    void send(Product& prod);
};

} // namespace

#endif /* MAIN_MCAST_MCASTSENDER_H_ */
