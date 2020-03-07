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

/******************************************************************************/

struct P2pInfo
{
    SockAddr   sockAddr;    ///< Server's socket address
    PortPool   portPool;    ///< Pool of port numbers for transitory servers
    int        listenSize;  ///< Server's `::listen()` size
    int        maxPeers;    ///< Maximum number of peers
};

/******************************************************************************/

class P2pMgr; // Forward declaration

/**
 * Interface for an observer of peer changes.
 */
class PeerChngObs
{
public:
    virtual ~PeerChngObs() noexcept;

    /**
     * Accepts notification that a local peer has been added.
     *
     * @param[in] peer        Local peer
     * @throws    logicError  Shouldn't have been called
     */
    virtual void added(Peer& peer);

    /**
     * Accepts notification that a local peer has been removed.
     *
     * @param[in] peer        Local peer
     * @throws    logicError  Shouldn't have been called
     */
    virtual void removed(Peer& peer);
};

/**
 * Interface for an observer of a peer-to-peer manager.
 */
class P2pMgrObs : public PeerChngObs
{
public:
    virtual ~P2pMgrObs() noexcept =0;

    /**
     * Indicates if product-information should be requested.
     *
     * @param[in] prodIndex  identifier of product
     * @retval    `true`     The information should be requested
     * @retval    `false`    The information should not be requested
     */
    virtual bool shouldRequest(ProdIndex prodIndex) =0;

    /**
     * Indicates if a data-segment should be requested.
     *
     * @param[in] segId      Identifier of data-segment
     * @retval    `true`     The segment should be requested
     * @retval    `false`    The segment should not be requested
     */
    virtual bool shouldRequest(const SegId& segId) =0;

    /**
     * Returns product-information.
     *
     * @param[in] prodIndex   identifier of product
     * @return                The information. Will test false if it doesn't
     *                        exist.
     */
    virtual ProdInfo get(ProdIndex prodIndex) =0;

    /**
     * Returns a data-segment.
     *
     * @param[in] segId       Identifier of data-segment
     * @return                The segment. Will test false if it doesn't exist.
     */
    virtual MemSeg get(const SegId& segId) =0;

    /**
     * Accepts product-information.
     *
     * @param[in] prodInfo    Product information
     * @retval    `true`      Product information was accepted
     * @retval    `false`     Product information was previously accepted
     * @throws    logicError  Shouldn't have been called
     */
    virtual bool hereIsP2p(const ProdInfo& prodInfo) =0;

    /**
     * Accepts a data-segment.
     *
     * @param[in] tcpSeg      TCP-based data-segment
     * @retval    `true`      Chunk was accepted
     * @retval    `false`     Chunk was previously accepted
     * @throws    logicError  Shouldn't have been called
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
     * Constructs. Calls `::listen()`.
     *
     * @param[in] p2pInfo      Peer-to-peer execution parameters
     * @param[in] p2pSrvrPool  Pool of remote P2P servers
     * @param[in] p2pMgrObs    Observer of this peer-to-peer manager
     */
    P2pMgr(
            P2pInfo&    p2pInfo,
            ServerPool& p2pSrvrPool,
            P2pMgrObs&  p2pMgrObs);

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
     * Executes this instance (i.e., executes receiving threads and calls
     * the observer when appropriate). Returns if
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
     * Notifies all the managed peers about available product-information.
     *
     * @param[in] prodIndex  Identifier of product
     */
    void notify(ProdIndex prodIndex);

    /**
     * Notifies all the managed peers about an available data-segment.
     *
     * @param[in] segId  Identifier of data-segment
     */
    void notify(const SegId& segId);

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
