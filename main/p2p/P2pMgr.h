/**
 * This file declares the interface for a peer-to-peer manager. Such a manager
 * is called by peers to handle received PDU-s.
 * 
 * @file:   P2pMgr.h
 * @author: Steven R. Emmerson
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
    using PeerType = PubPeer;
    using Pimpl    = std::shared_ptr<P2pMgr>;

    /**
     * Relationship to the data-products:
     */
    enum class Type : char {
        UNSET,
        PUBLISHER,  // Publisher's P2P manager
        SUBSCRIBER  // Subscriber's P2P manager
    };

    ///< Peer-to-peer runtime parameters
    struct RunPar {
        struct Srvr {
            SockAddr addr;       ///< Socket address
            int      listenSize; ///< Size of `::listen()` queue
            Srvr(   const SockAddr addr,
                    const int      listenSize)
                : addr(addr)
                , listenSize(listenSize)
            {}
        }         srvr;           ///< P2P server
        int       maxPeers;       ///< Maximum number of connected peers
        int       trackerSize;    ///< Maximum size of list of P2P server's
        int       evalTime;       ///< Time interval over which to evaluate performance of peers in ms
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
     * Creates a publishing P2P manager.
     *
     * @param[in] pubNode       Hycast publishing node
     * @param[in] peerSrvrAddr  P2P server's socket address. It shall specify a specific interface
     *                          and not the wildcard. The port number may be 0, in which case the
     *                          operating system will choose the port.
     * @param[in] maxPeers      Maximum number of subscribing peers
     * @param[in] listenSize    Size of `::listen()` queue. 0 obtains the system default.
     * @throw InvalidArgument   `listenSize` is zero
     * @return                  Publisher's P2P manager
     * @see `run()`
     */
    static Pimpl create(
            PubNode&       pubNode,
            const SockAddr peerSrvrAddr,
            unsigned       maxPeers,
            const unsigned listenSize = 8);

    /**
     * Destroys.
     */
    virtual ~P2pMgr() noexcept {};

    /**
     * Starts this instance. Starts internal threads that create, accept, and execute peers. Doesn't
     * block. Must be paired with `stop()` only.
     *
     * @throw LogicError  Instance can't be re-executed
     * @see `stop()`
     */
    virtual void start() =0;

    /**
     * Stops execution. Does nothing if this instance hasn't been started. Blocks until execution
     * terminates. Rethrows the first unrecoverable exception thrown by an internal thread if one
     * exists. Must be paired with `start()` only.
     *
     * @throw SystemError   System failure
     * @throw RuntimeError  P2p server failure
     * @see `start()`
     */
    virtual void stop() =0;

    /**
     * Executes this instance. Starts internal threads that create, accept, and execute peers.
     * Doesn't return until `halt()` is called or an internal thread throws an unrecoverable
     * exception. Rethrows the first unrecoverable exception thrown by an internal thread if one
     * exists. Must be paired with `halt()` only.
     *
     * @throw LogicError    Instance can't be re-executed
     * @throw SystemError   System failure
     * @throw RuntimeError  P2p server failure
     * @see `halt()`
     */
    virtual void run() =0;

    /**
     * Halts execution. Does nothing if this instance isn't executing. Causes `run()` to return.
     * Doesn't block. Must be paired with `run()` only.
     *
     * @see `run()`
     */
    virtual void halt() =0;

    /**
     * Returns the address of this instance's peer-server. This function exists
     * to support testing.
     *
     * @return  Address of the peer-server
     */
    virtual SockAddr getSrvrAddr() const =0;

    /**
     * Blocks until at least one remote peer has established a connection via
     * the local peer-server. Useful for unit-testing.
     */
    virtual void waitForSrvrPeer() =0;

    /**
     * Notifies connected remote peers about the availability of product
     * information.
     *
     * @param[in] prodInfo  Product information
     */
    virtual void notify(const ProdIndex prodIndex) =0;

    /**
     * Notifies connected remote peers about the availability of a data
     * segment.
     *
     * @param[in] dataSegId  Data segment ID
     */
    virtual void notify(const DataSegId dataSegId) =0;

    /**
     * Receives a request for product information from a remote peer.
     *
     * @param[in] request      Which product
     * @param[in] rmtAddr      Socket address of remote peer
     * @return                 Product information. Will test false if it
     *                         shouldn't be sent to remote peer.
     */
    virtual ProdInfo recvRequest(
            const ProdIndex request,
            const SockAddr  rmtAddr) =0;
    /**
     * Receives a request for a data-segment from a remote peer.
     *
     * @param[in] request      Which data-segment
     * @param[in] rmtAddr      Socket address of remote peer
     * @return                 Product information. Will test false if it
     *                         shouldn't be sent to remote peer.
     */
    virtual DataSeg  recvRequest(
            const DataSegId request,
            const SockAddr  rmtAddr) =0;
};

using PubP2pMgr = P2pMgr;

/**************************************************************************************************/

/// Interface for a subscriber's P2P manager.
class SubP2pMgr : public P2pMgr
{
public:
    using PeerType  = SubPeer;
    using Pimpl     = std::shared_ptr<SubP2pMgr>;

    /**
     * Creates a subscribing P2P manager.
     *
     * @param[in] subNode   Subscriber's node
     * @param[in] tracker   Socket addresses of potential peer-servers
     * @param[in] p2pAddr   Socket address of subscriber's P2P server. IP address *must not* specify
     *                      all interfaces. If port number is 0, then O/S will choose.
     * @param[in] maxPeers  Maximum number of peers. Might be adjusted upwards.
     * @param[in] segSize   Size, in bytes, of canonical data-segment
     * @return              Subscribing P2P manager
     * @see `getPeerSrvrAddr()`
     */
    static Pimpl create(
            SubNode&       subNode,
            Tracker        tracker,
            const SockAddr p2pAddr,
            const unsigned maxPeers,
            const SegSize  segSize);

    /**
     * Destroys.
     */
    virtual ~SubP2pMgr() noexcept =default;

    /**
     * Receives a notice of available product information from a remote peer.
     *
     * @param[in] notice       Which product
     * @param[in] rmtAddr      Socket address of remote peer
     * @retval    `false`      Local peer shouldn't request from remote peer
     * @retval    `true`       Local peer should request from remote peer
     */
    virtual bool recvNotice(const ProdIndex  notice,
                            const SockAddr   rmtAddr) =0;
    /**
     * Receives a notice of an available data-segment from a remote peer.
     *
     * @param[in] notice       Which data-segment
     * @param[in] rmtAddr      Socket address of remote peer
     * @retval    `false`      Local peer shouldn't request from remote peer
     * @retval    `true`       Local peer should request from remote peer
     */
    virtual bool recvNotice(const DataSegId notice,
                            const SockAddr  rmtAddr) =0;

    /**
     * Handles a request for data-product information not being satisfied by a
     * remote peer.
     *
     * @param[in] prodIndex  Index of the data-product
     * @param[in] rmtAddr    Socket address of remote peer
     */
    virtual void missed(
            const ProdIndex prodIndex,
            SockAddr        rmtAddr) =0;

    /**
     * Handles a request for a data-segment not being satisfied by a remote
     * peer.
     *
     * @param[in] dataSegId  ID of data-segment
     * @param[in] rmtAddr    Socket address of remote peer
     */
    virtual void missed(const DataSegId dataSegId,
                        SockAddr        rmtAddr) =0;

    /**
     * Receives a set of potential peer servers from a remote peer.
     *
     * @param[in] tracker      Set of potential peer-servers
     * @param[in] rmtAddr      Socket address of remote peer
     */
    virtual void recvData(const Tracker   tracker,
                          const SockAddr  rmtAddr) =0;
    /**
     * Receives the address of a potential peer-server from a remote peer.
     *
     * @param[in] srvrAddr     Socket address of potential peer-server
     * @param[in] rmtAddr      Socket address of remote peer
     */
    virtual void recvData(const SockAddr srvrAddr,
                          const SockAddr rmtAddr) =0;
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
