/**
 * 
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Node.h
 *  Created on: Jun 3, 2020
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_NODE_NODE_H_
#define MAIN_NODE_NODE_H_

#include "McastProto.h"
#include "P2pMgr.h"

#include <memory>

namespace hycast {

/**
 * A node on a data-product distribution network.
 */
class Node
{
public:
    class Impl;

protected:
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Constructs a publishing node.
     *
     * @param[in] p2pInfo  Information about the local P2P server
     * @param[in] grpAddr  Address to which products will be multicast
     * @param[in] repoDir  Pathname of root of product repository
     * @param[in] p2pObs   Observer of the P2P network
     */
    Node(   P2pInfo&           p2pInfo,
            const SockAddr&    grpAddr,
            const std::string& repoDir,
            P2pObs&            p2pObs);

    /**
     * Constructs a subscribing node.
     *
     * @param[in] srcMcastInfo  Source-specific multicast information
     * @param[in] p2pInfo       Information about the local P2P server
     * @param[in] ServerPool    Pool of remote P2P servers
     * @param[in] repoDir       Pathname of root of product repository
     * @param[in] segSize       Size, in bytes, of a canonical data-segment
     * @param[in] p2pObs        Observer of the P2P network
     */
    Node(   const SrcMcastAddrs& srcMcastInfo,
            P2pInfo&             p2pInfo,
            ServerPool&          p2pSrvrPool,
            const std::string&   repoDir,
            const SegSize        segSize,
            P2pObs&              p2pObs);

    /**
     * Executes this instance.
     */
    void operator()() const;

    /**
     * Halts execution of this instance. Causes `operator()()` to return.
     */
    void halt() const;
};

} // namespace

#endif /* MAIN_NODE_NODE_H_ */
