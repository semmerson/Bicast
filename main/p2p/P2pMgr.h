/**
 * This file declares the interface for a peer-to-peer manager. Such a manager is called by peers to
 * handle received PDU-s.
 * 
 * @file:   P2pMgr.h
 * @author: Steven R. Emmerson
 *
 *    Copyright 2021 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License
 * is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied. See the License for the specific language governing permissions and limitations under
 * the License.
 */

#ifndef MAIN_P2P_P2PMGR_H_
#define MAIN_P2P_P2PMGR_H_

#include "error.h"
#include "HycastProto.h"
#include "Tracker.h"

#include <cstdint>
#include <memory>
#include <string>

namespace hycast {

class Peer;    // Forward declaration
class PubPeer; // Forward declaration
class SubPeer; // Forward declaration

class PubNode;
class SubNode;

/**
 * Interface for a peer-to-peer manager. A publisher's P2P manager will only implement this
 * interface.
 */
class P2pMgr
{
public:
    /// Type of this peer
    using PeerType = PubPeer;
    /// Smart pointer to the implementation
    using Pimpl    = std::shared_ptr<P2pMgr>;

    /**
     * Relationship to the data-products:
     */
    enum class Type : char {
        UNSET,
        PUBLISHER,  // Publisher's P2P manager
        SUBSCRIBER  // Subscriber's P2P manager
    };

    /// Peer-to-peer runtime parameters
    struct RunPar {
        /// Runtime parameters for the P2P server
        struct Srvr {
            SockAddr addr;        ///< Socket address
            int      acceptQSize; ///< Size of `listen()` queue
            /**
             * Constructs.
             * @param[in] addr         Address for local P2P server
             * @param[in] listenSize   Size of listen() queue
             */
            Srvr(   const SockAddr addr,
                    const int      listenSize)
                : addr(addr)
                , acceptQSize(listenSize)
            {}
        }         srvr;           ///< P2P server
        int       maxPeers;       ///< Maximum number of connected peers
        int       trackerSize;    ///< Maximum size of pool of potential P2P server addresses
        int       evalTime;       ///< Time interval for evaluating peer performance in seconds
        /**
         * Constructs.
         * @param[in] addr         Address for local P2P server
         * @param[in] listenSize   Size of listen() queue
         * @param[in] maxPeers     Maximum number of neighboring peers to have
         * @param[in] trackerSize  Maximum size of pool of potential P2P server addresses
         * @param[in] evalTime     Time interval for evaluating peer performance in seconds
         */
        RunPar( const SockAddr addr,
                const int      listenSize,
                const int      maxPeers,
                const int      trackerSize,
                const int      evalTime)
            : srvr(addr, listenSize)
            , maxPeers(maxPeers)
            , trackerSize(trackerSize)
            , evalTime(evalTime)
        {}
    };

    /**
     * Creates a publishing P2P manager. Creates a P2P server listening on a socket but doesn't do
     * anything with it until `run()` is called.
     *
     * @param[in] pubNode       Hycast publishing node
     * @param[in] peerSrvrAddr  P2P server's socket address. It shall specify a specific interface
     *                          and not the wildcard. The port number may be 0, in which case the
     *                          operating system will choose the port.
     * @param[in] maxPeers      Maximum number of subscribing peers
     * @param[in] listenSize    Size of listening queue. 0 obtains the system default.
     * @param[in] evalTime      Evaluation interval for poorest-performing peer in seconds
     * @throw InvalidArgument   `listenSize` is zero
     * @return                  Publisher's P2P manager
     * @see `run()`
     */
    static Pimpl create(
            PubNode&       pubNode,
            const SockAddr peerSrvrAddr,
            unsigned       maxPeers,
            const unsigned listenSize,
            const unsigned evalTime);

    /**
     * Destroys.
     */
    virtual ~P2pMgr() noexcept {};

    /**
     * Executes this instance. Starts internal threads that create, accept, and execute peers.
     * Doesn't return until `halt()` is called or an internal thread throws an  exception. Rethrows
     * the first exception thrown by an internal thread if it exists.
     *
     * @throw LogicError    Function called more than once
     * @throw SystemError   System failure
     * @throw RuntimeError  P2p server failure
     * @see                 `halt()`
     */
    virtual void run() =0;

    /**
     * Halts execution. Causes `run()` to return. Doesn't block.
     *
     * @asyncsignalsafe  Yes
     * @see              `run()`
     */
    virtual void halt() =0;

    /**
     * Returns the address of this instance's P2P-server.
     *
     * @return  Address of the P2P-server
     */
    virtual SockAddr getSrvrAddr() const =0;

    /**
     * Blocks until at least one remote peer has established a connection via the local peer-server.
     * Useful for unit-testing.
     */
    virtual void waitForSrvrPeer() =0;

    /**
     * Receives notification as to whether a remote P2P node provides a path to the publisher.
     *
     * @param[in] havePubPath  Does the remote P2P node provide a path to the publisher?
     * @param[in] rmtAddr      Socket address of associated remote peer
     */
    virtual void recvHavePubPath(
            const bool     havePubPath,
            const SockAddr rmtAddr) {};

    /**
     * Receives the address of a potential peer-server from a remote peer.
     *
     * @param[in] p2pSrvr     Socket address of potential peer-server
     */
    virtual void recvAdd(const SockAddr p2pSrvr) {}; // Default implemented so mocks don't have to
    /**
     * Receives a set of potential peer servers from a remote peer.
     *
     * @param[in] tracker      Set of potential peer-servers
     */
    virtual void recvAdd(Tracker tracker) {};

    /**
     * Receives the address of a bad peer-server from a remote peer.
     *
     * @param[in] p2pSrvr     Socket address of potential peer-server
     */
    virtual void recvRemove(const SockAddr p2pSrvr) {};
    /**
     * Receives a set of bad peer servers from a remote peer.
     *
     * @param[in] tracker      Set of bad peer-servers
     */
    virtual void recvRemove(const Tracker tracker) {};

    /**
     * Notifies connected remote peers about the availability of product information.
     *
     * @param[in] prodId  Product identifier
     */
    virtual void notify(const ProdId prodId) =0;

    /**
     * Notifies connected remote peers about the availability of a data
     * segment.
     *
     * @param[in] dataSegId  Data segment ID
     */
    virtual void notify(const DataSegId dataSegId) =0;

    /**
     * Returns a set of this instance's identifiers of complete products minus those of another set.
     *
     * @param[in]  rhs      Other set of product identifiers to be subtracted from the ones this
     *                      instance has
     * @return              This instance's identifiers minus those of the other set
     */
    virtual ProdIdSet subtract(ProdIdSet rhs) const =0;

    /**
     * Returns the set of identifiers of complete products.
     *
     * @return             Set of complete product identifiers
     */
    virtual ProdIdSet getProdIds() const =0;

    /**
     * Returns information on a product. This might count against the remote peer.
     *
     * @param[in] prodId       Which product
     * @param[in] rmtAddr      Socket address of remote peer
     * @return                 Product information. Will test false if it
     *                         shouldn't be sent to remote peer.
     */
    virtual ProdInfo getDatum(
            const ProdId   prodId,
            const SockAddr rmtAddr) =0;
    /**
     * Returns a data-segment. This might count against the remote peer.
     *
     * @param[in] dataSegId    Which data-segment
     * @param[in] rmtAddr      Socket address of remote peer
     * @return                 Product information. Will test false if it
     *                         shouldn't be sent to remote peer.
     */
    virtual DataSeg getDatum(
            const DataSegId dataSegId,
            const SockAddr  rmtAddr) =0;
};

using PubP2pMgr = P2pMgr; ///< Type of publisher's P2P manager

/**************************************************************************************************/

/// Interface for a subscriber's P2P manager.
class SubP2pMgr : public P2pMgr
{
public:
    using PeerType  = SubPeer; ///< Type of peer handled by this class
    /// Smart pointer to the implementation
    using Pimpl     = std::shared_ptr<SubP2pMgr>;

    /**
     * Creates a subscribing P2P manager. Creates a P2P server listening on a socket but doesn't do
     * anything with it until `run()` is called.
     *
     * @param[in] subNode      Subscriber's node
     * @param[in] tracker      Pool of addresses of P2P servers
     * @param[in] p2pSrvr      Server socket for subscriber's P2P server. IP address *must not* be
     *                         wildcard.
     * @param[in] acceptQSize  Maximum number of outstanding, incoming, Hycast connections
     * @param[in] maxPeers     Maximum number of peers. Might be adjusted upwards.
     * @param[in] evalTime     Evaluation interval for poorest-performing peer in seconds
     * @return                 Subscribing P2P manager
     * @see `getPeerSrvrAddr()`
    static Pimpl create(
            SubNode&          subNode,
            Tracker           tracker,
            const TcpSrvrSock p2pSrvr,
            const int         acceptQSize,
            const unsigned    maxPeers,
            const unsigned    evalTime);
     */

    /**
     * Creates a subscribing P2P manager. Creates a P2P server listening on a socket but doesn't do
     * anything with it until `run()` is called.
     *
     * @param[in] subNode      Subscriber's node
     * @param[in] tracker      Pool of addresses of P2P servers
     * @param[in] p2pSrvr      Socket address for subscriber's P2P server. IP address *must not* be
     *                         wildcard. If the port number is zero, then the O/S will choose an
     *                         ephemeral port number.
     * @param[in] acceptQSize  Maximum number of outstanding, incoming, Hycast connections
     * @param[in] timeout      Timeout, in ms, for connecting to remote P2P servers. -1 => default
     *                         timeout; 0 => immediate return.
     * @param[in] maxPeers     Maximum number of peers. Might be adjusted upwards.
     * @param[in] evalTime     Evaluation interval for poorest-performing peer in seconds
     * @return                 Subscribing P2P manager
     * @see `getPeerSrvrAddr()`
     */
    static Pimpl create(
            SubNode&          subNode,
            Tracker           tracker,
            const SockAddr    p2pSrvr,
            const int         acceptQSize,
            const int         timeout,
            const unsigned    maxPeers,
            const unsigned    evalTime);

    /**
     * Destroys.
     */
    virtual ~SubP2pMgr() noexcept =default;

    /**
     * Receives a notice of available product information from a remote peer.
     *
     * @param[in] notice       Which product
     * @param[in] rmtAddr      Socket address of remote peer
     * @retval    false        Local peer shouldn't request from remote peer
     * @retval    true         Local peer should request from remote peer
     */
    virtual bool recvNotice(const ProdId   notice,
                            const SockAddr rmtAddr) =0;
    /**
     * Receives a notice of an available data-segment from a remote peer.
     *
     * @param[in] notice       Which data-segment
     * @param[in] rmtAddr      Socket address of remote peer
     * @retval    false        Local peer shouldn't request from remote peer
     * @retval    true         Local peer should request from remote peer
     */
    virtual bool recvNotice(const DataSegId notice,
                            const SockAddr  rmtAddr) =0;

    /**
     * Handles a request for data-product information not being satisfied by a remote peer.
     *
     * @param[in] prodId     Index of the data-product
     * @param[in] rmtAddr    Socket address of remote peer
     */
    virtual void missed(
            const ProdId prodId,
            SockAddr     rmtAddr) =0;

    /**
     * Handles a request for a data-segment not being satisfied by a remote peer.
     *
     * @param[in] dataSegId  ID of data-segment
     * @param[in] rmtAddr    Socket address of remote peer
     */
    virtual void missed(const DataSegId dataSegId,
                        SockAddr        rmtAddr) =0;
    /**
     * Receives product information from a remote peer.
     *
     * @param[in] prodInfo  Product information
     * @param[in] rmtAddr   Socket address of remote peer
     */
    virtual void recvData(const ProdInfo prodInfo,
                          SockAddr       rmtAddr) =0;
    /**
     * Receives a data segment from a remote peer.
     *
     * @param[in] dataSeg  Data segment
     * @param[in] rmtAddr  Socket address of remote peer
     */
    virtual void recvData(const DataSeg dataSeg,
                          SockAddr      rmtAddr) =0;
};

} // namespace

#endif /* MAIN_P2P_P2PMGR_H_ */
