/**
 * Pool of threads for executing Peers.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: PeerThreadPool.h
 *  Created on: Aug 8, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_PEER_PEERTHREADPOOL_CPP_
#define MAIN_PEER_PEERTHREADPOOL_CPP_

#include "Peer.h"

#include <memory>

namespace hycast {

class PeerThreadPool
{
protected:
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Constructs.
     *
     * @param[in] numThreads             Number of threads in the pool
     */
    explicit PeerThreadPool(const size_t numThreads);

    PeerThreadPool(const PeerThreadPool& pool) =default;

    PeerThreadPool& operator=(const PeerThreadPool& rhs) =default;

    /**
     * Executes a peer.
     *
     * @param[in] peer     Peer to be executed
     * @retval    `true`   Success
     * @retval    `false`  Failure. All threads are busy.
     */
    bool execute(Peer peer);
};

} // namespace

#endif /* MAIN_PEER_PEERTHREADPOOL_CPP_ */
