/**
 * Subscriber node.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Subscriber.h
 *  Created on: Jan 13, 2020
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_RECEIVER_RECEIVER_H_
#define MAIN_RECEIVER_RECEIVER_H_

#include "McastProto.h"
#include "P2pMgr.h"
#include "PortPool.h"
#include "Repository.h"
#include "ServerPool.h"
#include "SockAddr.h"

#include <memory>

namespace hycast {

class Subscriber
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    Subscriber(Impl* const impl);

public:
    /**
     * Constructs.
     *
     * @param[in]     srcMcastInfo  Information on source-specific multicast
     * @param[in,out] p2pInfo       Information on peer-to-peer network
     * @param[in,out] p2pSrvrPool   Pool of potential peer-servers
     * @param[in,out] repo          Data-product repository
     * @param[in]     rcvrObs       Observer of this instance
     */
    Subscriber(
            const SrcMcastAddrs& srcMcastInfo,
            P2pInfo&             p2pInfo,
            ServerPool&          p2pSrvrPool,
            SubRepo&             repo,
            PeerChngObs&         rcvrObs);

    /**
     * Executes this instance. Doesn't return until either `halt()` is called
     * or an exception is thrown.
     *
     * @see `halt()`
     */
    void operator()() const;

    /**
     * Halts execution of this instance. Causes `operator()()` to return.
     *
     * @see `operator()()`
     */
    void halt() const;
};

} // namespace

#endif /* MAIN_RECEIVER_RECEIVER_H_ */
