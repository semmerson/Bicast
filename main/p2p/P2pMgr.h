/**
 * Creates and manages a peer-to-peer network.
 *
 *        File: PeerSetMgr.h
 *  Created on: Jul 1, 2019
 *      Author: Steven R. Emmerson
 *
 *    Copyright 2021 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MAIN_PEER_P2PMGR_H_
#define MAIN_PEER_P2PMGR_H_

#include "PeerSet.h"
#include "PortPool.h"
#include "SockAddr.h"
#include "ServerPool.h"

#include <memory>

namespace hycast {

/**
 * Interface for a sender on a P2P network.
 */
class P2pSndr
{
public:
    virtual ~P2pSndr() noexcept =default;

    /**
     * Returns product-information.
     *
     * @param[in] prodIndex   identifier of product
     * @return                The information. Will test false if it doesn't
     *                        exist.
     */
    virtual ProdInfo getProdInfo(ProdIndex prodIndex) =0;

    /**
     * Returns a data-segment.
     *
     * @param[in] segId       Identifier of data-segment
     * @return                The segment. Will test false if it doesn't exist.
     */
    virtual MemSeg getMemSeg(const SegId& segId) =0;
};

/******************************************************************************/

/**
 * Interface for a subscriber on a P2P network.
 */
class P2pSub : public P2pSndr
{
public:
    virtual ~P2pSub() noexcept =default;

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
    virtual bool hereIsP2p(TcpSeg& tcpSeg) =0;
};

/******************************************************************************/

/**
 * Information on a P2P server. Applicable to both a publisher and subscriber.
 */
struct P2pInfo
{
    SockAddr   sockAddr;    ///< Server's socket address
    int        listenSize;  ///< Server's `::listen()` size
    int        maxPeers;    ///< Maximum number of peers
};

/******************************************************************************/

/**
 * A manager of a peer-to-peer network.
 */
class P2pMgr
{
public:
    class Impl;

private:
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Default constructs.
     */
    P2pMgr();

    /**
     * Constructs a publisher's peer-to-peer manager. Calls `::listen()`.
     *
     * @param[in] p2pInfo       Peer-to-peer execution parameters
     * @param[in] p2pPub        Peer-to-peer publisher
     */
    P2pMgr( const P2pInfo& p2pInfo,
            P2pSndr&       p2pPub);

    /**
     * Constructs a subscriber's peer-to-peer manager. Calls `::listen()`.
     *
     * @param[in] p2pInfo       Peer-to-peer execution parameters
     * @param[in] p2pSrvrPool   Pool of remote P2P servers
     * @param[in] p2pSub        Peer-to-peer subscriber
     */
    P2pMgr( const P2pInfo& p2pInfo,
            ServerPool&    p2pSrvrPool,
            P2pSub&        p2pSub);

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
    void notify(const ProdIndex prodIndex) const;

    /**
     * Notifies all the managed peers about an available data-segment.
     *
     * @param[in] segId  Identifier of data-segment
     */
    void notify(const SegId& segId) const;

    /**
     * Halts execution of this instance. If called before `operator()`, then
     * this instance will never execute.
     *
     * @threadsafety     Safe
     * @exceptionsafety  Basic guarantee
     */
    void halt() const;
};
#if 0
/**
 * A manager of a publisher's peer-to-peer network.
 */
class PubP2pMgr final : public P2pMgr
{
    class Impl;

public:
    /**
     * Default constructs.
     */
    PubP2pMgr();

    /**
     * Constructs. Calls `::listen()`.
     *
     * @param[in] p2pInfo       Peer-to-peer execution parameters
     * @param[in] p2pPub        Peer-to-peer publisher
     */
    PubP2pMgr(
            P2pInfo&  p2pInfo,
            P2pSndr&   p2pPub);
};

/**
 * A manager of a subscriber's peer-to-peer network.
 */
class SubP2pMgr final : public P2pMgr
{
    class Impl;

public:
    /**
     * Default constructs.
     */
    SubP2pMgr();

    /**
     * Constructs. Calls `::listen()`.
     *
     * @param[in] p2pInfo       Peer-to-peer execution parameters
     * @param[in] p2pSrvrPool   Pool of remote P2P servers
     * @param[in] p2pSub        Peer-to-peer subscriber
     */
    SubP2pMgr(
            P2pInfo&      p2pInfo,
            ServerPool&   p2pSrvrPool,
            P2pSub&       p2pSub);
};
#endif

} // namespace

#endif /* MAIN_PEER_P2PMGR_H_ */
