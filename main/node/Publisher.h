/**
 * Publisher node.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Publisher.h
 *  Created on: Jan 3, 2020
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_NODE_PUBLISHER_H_
#define MAIN_NODE_PUBLISHER_H_

#include <main/inet/Socket.h>
#include "P2pMgr.h"
#include "Repository.h"

#include <memory>

namespace hycast {

class Publisher
{
protected:
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    Publisher(Impl* const impl);

public:
    /**
     * Constructs.
     *
     * @param[in] p2pSrvrInfo  Information on the P2P server
     * @param[in] grpAddr      Address of multicast group to which products will
     *                         be sent
     * @param[in] repoDir      Pathname of root-directory of repository
     */
    Publisher(
        P2pInfo&        p2pSrvrInfo,
        const SockAddr& grpAddr,
        std::string&    repoDir);

    void operator()() const;

    void halt() const;
};

} // namespace

#endif /* MAIN_NODE_PUBLISHER_H_ */
