/**
 * This file implements a component that ships data-products to receiving nodes
 * using both multicast and peer-to-peer transports.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Shipping.cpp
 * @author: Steven R. Emmerson
 */
#include "config.h"

#include "error.h"
#include "PeerSet.h"
#include "ProdStore.h"
#include "Shipping.h"

namespace hycast {

class Shipping::Impl final
{
    class PeerMgr final
    {
        PeerSet peerSet;
    public:
        PeerMgr(PeerSet& peerSet)
            : peerSet{peerSet}
        {}
        void notify(const Product& prod)
        {
            auto prodInfo = prod.getInfo();
            peerSet.sendNotice(prodInfo);
            ChunkIndex numChunks = prodInfo.getNumChunks();
            for (ChunkIndex i = 0; i < numChunks; ++i)
                peerSet.sendNotice(ChunkInfo{prodInfo, i});
        }
    }           peerMgr;
    ProdStore   prodStore;
    McastSender mcastSender;

public:
    /**
     * Constructs.
     * @param[in] prodStore    Product store
     * @param[in] mcastSender  Multicast sender.
     */
    Impl(   ProdStore&   prodStore,
            McastSender& mcastSender,
            PeerSet&     peerSet)
        : peerMgr{peerSet}
        , prodStore{prodStore}
        , mcastSender{mcastSender}
    {}

    /**
     * Ships a product.
     * @param[in] prod  Product to ship
     */
    void ship(Product& prod)
    {
        mcastSender.send(prod);
        prodStore.add(prod);
        peerMgr.notify(prod);
    }
};

Shipping::Shipping(
        ProdStore&   prodStore,
        McastSender& mcastSender,
        PeerSet&     peerSet)
    : pImpl{new Impl(prodStore, mcastSender, peerSet)}
{}

void Shipping::ship(Product& prod)
{
    pImpl->ship(prod);
}

} // namespace
