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
#include "Repository.h"

#include <memory>

namespace hycast {

/**
 * Abstract base class for a node on a data-product distribution network.
 */
class Node
{
protected:
    class Impl;

    std::shared_ptr<Impl> pImpl;

    Node(std::shared_ptr<Impl> pImpl);

public:
    virtual ~Node() =0;

    /**
     * Executes this instance.
     */
    void operator()() const /*=0*/;

    /**
     * Halts execution of this instance. Causes `operator()()` to return.
     */
    void halt() const /*=0*/;
};

class Publisher final : public Node
{
    class Impl;

public:
    /**
     * Constructs.
     *
     * @param[in] p2pInfo  Information about the local P2P server
     * @param[in] grpAddr  Address to which products will be multicast
     * @param[in] repo     Publisher's repository
     */
    Publisher(
            P2pInfo&        p2pInfo,
            const SockAddr& grpAddr,
            PubRepo&        repo);

    /**
     * Links to a file (which could be a directory) that's outside the
     * repository. All regular files will be published.
     *
     * @param[in] pathname       Absolute pathname (with no trailing '/') of the
     *                           file or directory to be linked to
     * @param[in] prodName       Product name if the pathname references a file
     *                           and Product name prefix if the pathname
     *                           references a directory
     * @throws InvalidArgument  `pathname` is empty or a relative pathname
     * @throws InvalidArgument  `prodName` is invalid
     */
    void link(
            const std::string& pathname,
            const std::string& prodName);

#if 0
    /**
     * Executes this instance.
     */
    void operator()() const;
#endif
};

class Subscriber final : public Node
{
    class Impl;

public:
    /**
     * Constructs.
     *
     * @param[in] srcMcastInfo  Source-specific multicast information
     * @param[in] p2pInfo       Information about the local P2P server
     * @param[in] ServerPool    Pool of remote P2P servers
     * @param[in] repoDir       Pathname of root of product repository
     */
    Subscriber(
            const SrcMcastAddrs& srcMcastInfo,
            P2pInfo&             p2pInfo,
            ServerPool&          p2pSrvrPool,
            SubRepo&             repo);

#if 0
    /**
     * Executes this instance.
     */
    void operator()() const;
#endif
};

} // namespace

#endif /* MAIN_NODE_NODE_H_ */
