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

    Node(Impl* const impl);

public:
    /**
     * Constructs a publishing node.
     *
     * @param[in] p2pSrvrInfo  Information on the P2P server
     * @param[in] grpAddr      Address to which products will be multicast
     * @param[in] repoDir      Pathname of root of product repository
     */
    Node(   P2pInfo&           p2pSrvrInfo,
            const SockAddr&    grpAddr,
            const std::string& repoDir);

    /**
     * Constructs a subscribing node.
     *
     * @param[in] srcMcastInfo  Source-specific multicast information
     * @param[in] p2pInfo       P2P server information
     * @param[in] ServerPool    Pool of remote P2P servers
     * @param[in] repoDir       Pathname of root of product repository
     * @param[in] segSize       Size, in bytes, of a canonical data-segment
     */
    Node(   const SrcMcastAddrs& srcMcastInfo,
            P2pInfo&             p2pInfo,
            ServerPool&          p2pSrvrPool,
            const std::string&   repoDir,
            const SegSize        segSize);

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
