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

#include "PortPool.h"
#include "PeerSet.h"
#include "ServerPool.h"
#include "SockAddr.h"

#include <memory>

namespace hycast {

class P2pMgr; // Forward declaration

/**
 * Interface for an observer of a peer-to-peer manager.
 */
class P2pMgrObs
{
public:
    virtual ~P2pMgrObs() noexcept
    {}

    /**
     * Accepts notification that a local peer has been added.
     *
     * @param[in] peer  The local peer
     */
    virtual void added(Peer& peer) =0;

    /**
     * Accepts notification that a local peer has been removed.
     *
     * @param[in] peer  The local peer
     */
    virtual void removed(Peer& peer) =0;

    /**
     * Indicates if a chunk should be requested.
     *
     * @param[in] chunkId    Chunk Identifier
     * @retval    `true`     The chunk should be requested
     * @retval    `false`    The chunk should not be requested
     */
    virtual bool shouldRequest(const ChunkId chunkId) =0;

    /**
     * Returns a chunk.
     *
     * @param[in] chunkId    Chunk Identifier
     * @return               The chunk. Will test false if it doesn't exist.
     */
    virtual const Chunk& get(const ChunkId chunkId) =0;

    /**
     * Accepts product-information.
     *
     * @param[in] prodInfo  Product information
     * @retval    `true`    Product information was accepted
     * @retval    `false`   Product information was previously accepted
     */
    virtual bool hereIs(const ProdInfo& prodInfo) =0;

    /**
     * Accepts a data-segment.
     *
     * @param[in] tcpSeg   TCP-based data-segment
     * @retval    `true`   Chunk was accepted
     * @retval    `false`  Chunk was previously accepted
     */
    virtual bool hereIs(TcpSeg& tcpSeg) =0;
};

/**
 * Manager of a peer-to-peer network.
 */
class P2pMgr
{
protected:
    class Impl;

    /**
     * Constructs from an implementation.
     *
     * @param[in] impl  The implementation
     */
    P2pMgr(Impl* const impl);

private:
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Default constructs.
     */
    P2pMgr();

    /**
     * Constructs. Calls `::listen()`.
     *
     * @param[in] srvrAddr    Socket address of local server that accepts
     *                        connections from remote peers
     * @param[in] listenSize  Size of server's `::listen()` queue
     * @param[in] portPool    Pool of available port numbers for the local
     *                        server to use when necessary
     * @param[in] maxPeers    Maximum number of active peers in the set
     * @param[in] serverPool  Pool of possible remote servers for remote peers
     * @param[in] p2pMgrObs   Observer of this peer-to-peer manager
     */
    P2pMgr( const SockAddr& srvrAddr,
            const int       listenSize,
            PortPool&       portPool,
            const int       maxPeers,
            ServerPool&     serverPool,
            P2pMgrObs&      p2pMgrObs);

    /**
     * Sets the time period over which this instance will attempt to replace the
     * worst performing peer in a full set of peers.
     *
     * @param[in] timePeriod  Amount of time in seconds for the improvement
     *                        period and also the minimum amount of time before
     *                        the peer-server associated with a failed remote
     *                        peer is re-connected to
     * @return                This instance
     */
    P2pMgr& setTimePeriod(unsigned timePeriod);

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
     * Notifies all the managed peers about an available chunk.
     *
     * @param[in] chunkId  Chunk identifier
     */
    void notify(const ChunkId chunkId);

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
