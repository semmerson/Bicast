/**
 * Creates and manages a peer-to-peer network.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: PeerSetMgr.h
 *  Created on: Jul 1, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_PEER_P2PMGR_H_
#define MAIN_PEER_P2PMGR_H_

#include "PeerSet.h"
#include "PortPool.h"
#include "ServerPool.h"

#include <memory>

namespace hycast {

class P2pMgr
{
public:
    class Impl;

private:
    std::shared_ptr<Impl> pImpl;

protected:
    /**
     * Constructs from an implementation.
     *
     * @param[in] impl  The implementation
     */
    P2pMgr(Impl* const impl);

public:
    /**
     * Default constructs.
     */
    P2pMgr();

    /**
     * Constructs.
     *
     * @param[in] srvrAddr    Socket address of local server that accepts
     *                        connections from remote peers
     * @param[in] listenSize  Size of server's `::listen()` queue
     * @param[in] portPool    Pool of available port numbers for the local
     *                        server to use when necessary
     * @param[in] maxPeers    Maximum number of active peers in the set
     * @param[in] serverPool  Pool of possible remote servers for remote peers
     * @param[in] msgRcvr     Receiver of messages from peers
     */
    P2pMgr( const SockAddr& srvrAddr,
            const int       listenSize,
            PortPool&       portPool,
            const int       maxPeers,
            ServerPool&     serverPool,
            PeerMsgRcvr&    msgRcvr);

    /**
     * Executes this instance. Returns if
     *   - `halt()` is called
     *   - An exception is thrown
     * Returns immediately if `halt()` was called before this method.
     *
     * @threadsafety     Safe
     * @exceptionsafety  Basic guarantee
     */
    void operator ()();

    /**
     * Returns the number of peers currently being managed.
     *
     * @return        Number of peers currently being managed
     * @threadsafety  Safe
     */
    size_t size() const;

    /**
     * Notifies all the managed peers of an available chunk.
     *
     * @param[in] chunkId  The available chunk
     * @retval    `true`   A notice was successfully enqueued for all peers
     * @retval    `false`  A notice was not successfully enqueued for at least
     *                     one peer
     */
    bool notify(const ChunkId& chunkId);

    /**
     * Halts execution of this instance. If called before `operator()`, then
     * this instance will never execute.
     *
     * @threadsafety     Safe
     * @exceptionsafety  Basic guarantee
     */
    void halt() const;
};

} // namespace

#endif /* MAIN_PEER_P2PMGR_H_ */
