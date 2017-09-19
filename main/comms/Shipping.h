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
#include "P2pMgr.h"
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
     * Constructs. Blocks until ready to accept an incoming connection from a
     * remote peer.
     * @param[in] prodStore       Product store
     * @param[in] mcastAddr       Multicast group socket address
     * @param[in] version         Protocol version
     * @param[in] maxPeers        Maximum number of peers
     * @param[in] stasisDuration  Minimum amount of time that the set of active
     *                            peers must be unchanged before the
     *                            worst-performing peer may be replaced
     * @param[in] serverAddr      Socket address of local server for remote
     *                            peers
     * @see PeerSet(std::function<Peer&>, unsigned, unsigned)
     */
    Shipping(
            ProdStore&              prodStore,
            const InetSockAddr&     mcastAddr,
            const unsigned          version,
            const unsigned          maxPeers,
            const PeerSet::TimeUnit stasisDuration,
            const InetSockAddr&     serverAddr);

    /**
     * Constructs. The default maximum number of peers and default stasis
     * duration for the set of active peers will be used. Blocks until ready to
     * accept an incoming connection from a remote peer.
     * @param[in] prodStore       Product store
     * @param[in] mcastAddr       Multicast group socket address
     * @param[in] version         Protocol version
     * @param[in] serverAddr      Socket address of local server for remote
     *                            peers
     * @see Shipping(ProdStore, InetSockAddr, unsigned, unsigned, unsigned,
     * 			     InetSockAddr)
     */
    Shipping(
            ProdStore&          prodStore,
            const InetSockAddr& mcastAddr,
            const unsigned      version,
            const InetSockAddr& serverAddr)
    	: Shipping(prodStore, mcastAddr, version, PeerSet::defaultMaxPeers,
                PeerSet::TimeUnit{60}, serverAddr)
    {}

    /**
     * Ships a product.
     * @param[in] prod  Product to ship
     */
    void ship(Product& prod);
};

} // namespace

#endif /* MAIN_COMMS_SHIPPING_H_ */
