/**
 * This file declares a component that ships data-products to receiving nodes
 * using both multicast and peer-to-peer transports.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Shipping.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_COMMS_SHIPPING_H_
#define MAIN_COMMS_SHIPPING_H_

#include "McastSender.h"
#include "PeerSet.h"
#include "ProdStore.h"
#include "Product.h"

#include <memory>

namespace hycast {

class Shipping final
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Constructs.
     * @param[in] prodStore    Product store
     * @param[in] mcastSender  Multicast sender
     * @param[in] peerSet      Empty set of P2P peers
     * @param[in] serverAddr   Socket address of local server for remote peers
     */
    Shipping(
            ProdStore&          prodStore,
            McastSender&        mcastSender,
            PeerSet&            peerSet,
			const InetSockAddr& serverAddr);

    /**
     * Ships a product.
     * @param[in] prod  Product to ship
     */
    void ship(Product& prod);
};

} // namespace

#endif /* MAIN_COMMS_SHIPPING_H_ */
