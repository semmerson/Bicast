/**
 * This file declares the Peer class. A peer handles low-level, asynchronous, bidirectional
 * messaging with its remote counterpart.
 *
 *  @file:  Peer.h
 * @author: Steven R. Emmerson <emmerson@ucar.edu>
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

#ifndef MAIN_PROTO_PEER_H_
#define MAIN_PROTO_PEER_H_

#include "HycastProto.h"
#include "P2pMgr.h"
#include "PeerConn.h"
#include "Socket.h"

#include <memory>

namespace hycast {

class Peer;                            ///< Forward declaration
using PeerPtr = std::shared_ptr<Peer>; ///< Smart pointer to an implementation

/**
 * Interface for a peer.
 */
class Peer
{
public:
    /**
     * Returns a publisher's server-side implementation. The resulting peer is fully connected and
     * ready for `xchgSrvrInfo()`, and then `run()` to be called.
     *
     * @param[in] p2pMgr           Associated publisher's P2P manager
     * @param[in] conn             Connection with remote peer
     * @see       xchgSrvrInfo()
     * @see       run()
     */
    static PeerPtr create(
            PubP2pMgr&  p2pMgr,
            PeerConnPtr conn);

    /**
     * Returns a subscriber's server-side implementation. The resulting peer is fully connected and
     * ready for `xchgSrvrInfo()`, and then `run()` to be called.
     *
     * @param[in] p2pMgr           Associated subscriber's P2P manager
     * @param[in] conn             Connection with remote peer
     * @see       xchgSrvrInfo()
     * @see       run()
     */
    static PeerPtr create(
            SubP2pMgr&   p2pMgr,
            PeerConnPtr& conn);

    /**
     * Returns a subscriber's client-side implementation. The resulting peer is fully connected and
     * ready for `xchgSrvrInfo()`, and then `run()` to be called.
     *
     * @param[in] p2pMgr           Associated subscriber's P2P manager
     * @param[in] srvrAddr         Address of remote P2P-server
     * @throw     LogicError       Destination port number is zero
     * @throw     SystemError      Couldn't connect. Bad failure.
     * @throw     RuntimeError     Couldn't connect. Might be temporary.
     * @see       xchgSrvrInfo()
     * @see       run()
     */
    static PeerPtr create(
            SubP2pMgr&      p2pMgr,
            const SockAddr& srvrAddr);

    virtual ~Peer()
    {}

    /**
     * Receives information on the remote P2P-server. Called by the RPC layer. The information can
     * be retrieved by calling `getRmtSrvrInfo()`.
     * @param[in] srvrInfo     Information on the remote P2P-server
     * @throw InvalidArgument  Remote P2p-server's information is invalid
     * @throw InvalidArgument  Remote tier number is invalid
     * @see getRmtSrvrInfo()
     */
    virtual void recv(const P2pSrvrInfo& srvrInfo) =0;

    /**
     * Receives a tracker. Passes the information to the peer manager. Called by the RPC layer.
     * @param[in] tracker  Information on P2P servers
     */
    virtual void recv(const Tracker& tracker) =0;

    /**
     * Returns information on the remote P2P-server. This function may be called immediately after
     * construction. Called by the P2P manager.
     * @return        Information on the remote P2P-server
     * @threadsafety  Compatible but unsafe
     * @see setRmtSrvrInfo()
     */
    virtual P2pSrvrInfo getRmtSrvrInfo() noexcept =0;

    /**
     * Indicates if this instance was constructed as a client (i.e., it initiated the connection).
     *
     * @retval true     Constructed as a client
     * @retval false    Constructed by a server
     */
    virtual bool isClient() const noexcept =0;

    /**
     * Returns the socket address of the local peer.
     *
     * @return Socket address of local peer
     */
    virtual SockAddr getLclAddr() const noexcept =0;

    /**
     * Returns the socket address of the remote peer. This will be the socket address of the remote
     * P2P-server iff this peer was constructed client-side (i.e., initiated the connection).
     *
     * @return Socket address of remote peer
     */
    virtual SockAddr getRmtAddr() const noexcept =0;

    /**
     * Exchanges information on the local and remote P2P-servers with the remote peer.
     * @param[in] srvrInfo  Information on the local P2P-server
     * @param[in] tracker   Information on known P2P-servers
     * @retval true         Success
     * @retval false        Lost connection
     */
    virtual bool xchgSrvrInfo(
            const P2pSrvrInfo& srvrInfo,
            Tracker&           tracker) =0;

    /**
     * Indicates if the remote peer is a publisher (and not a subscriber).
     * @retval true   The remote peer is a publisher
     * @retval false  The remote peer is not a publisher
     */
    virtual bool isRmtPub() const noexcept =0;

    /**
     * Returns the number of hops to the publisher from this instance.
     * @return     The number of hops to the publisher from this instance
     * @retval -1  The number of hops is unknown
     */
    virtual P2pSrvrInfo::Tier getTier() const noexcept =0;

    /**
     * Returns the hash code of this instance.
     * @return The hash code of this instance
     */
    virtual size_t hash() const noexcept =0;

    /**
     * Indicates if this instance is less than another.
     * @param[in] rhs      The other, right-hand-side instance
     * @retval    true     This instance is less than the other
     * @retval    false    This instance is not less than the other
     */
    virtual bool operator<(const Peer& rhs) const noexcept =0;

    /**
     * Indicates if this instance is equal to another.
     * @param[in] rhs      The other, right-hand-side instance
     * @retval    true     This instance is equal to the other
     * @retval    false    This instance is not equal to the other
     */
    virtual bool operator==(const Peer& rhs) const noexcept =0;

    /**
     * Indicates if this instance is not equal to another.
     * @param[in] rhs      The other, right-hand-side instance
     * @retval    true     This instance is not equal to the other
     * @retval    false    This instance is equal to the other
     */
    virtual bool operator!=(const Peer& rhs) const noexcept =0;

    /**
     * Returns the string representation of this instance. Currently, it's the
     * string representation of the socket address of the remote peer.
     *
     * @return String representation of this instance
     */
    virtual String to_string() const =0;

    /**
     * Starts receiving messages from the remote peer. Doesn't return until
     *   - The connection with the remote peer is lost; or
     *   - `halt()` is called.
     *
     * @throw std::exception  An error occurred
     */
    virtual void run() =0;

    /**
     * Halts this instance. Causes `run()` to return.
     *
     * @asyncsignalsafe  Safe
     */
    virtual void halt() =0;

    /**
     * Tells the remote peer to add some potential P2P-servers.
     *
     * @param[in] tracker  P2P-servers to add
     */
    virtual void add(const Tracker& tracker) =0;

    /**
     * Notifies the remote peer about the local P2P-server.
     * @param[in] srvrInfo  Information on the local P2P-server
     */
    virtual void notify(const P2pSrvrInfo& srvrInfo) =0;
    /**
     * Notifies the remote peer about available information on a data-product.
     *
     * @throw     LogicError  Remote peer is publisher
     * @throw     LogicError  `start()` not yet called
     * @see       `start()`
     */
    virtual void notify(const ProdId    prodId) =0;
    /**
     * Notifies the remote peer about an available data segment.
     * @param[in] dataSegId  ID of the data segment
     */
    virtual void notify(const DataSegId dataSegId) =0;

    /**
     * Receives the set of product-identifiers that the remote peer has for determination of the
     * backlog of products to notify the remote peer about. The remote peer will not be the
     * publisher.
     *
     * @param[in] prodIds  Product-identifiers
     * @throw LogicError   Remote peer is the publisher
     * @throw LogicError   The backlog is already being handled
     */
    virtual void recvHaveProds(ProdIdSet prodIds) =0;

    /**
     * Requests product information from the remote peer.
     *
     * @param[in] prodId  Product ID
     */
    virtual void request(const ProdId prodId) =0;

    /**
     * Requests a data-segment from the remote peer.
     *
     * @param[in] segId  Data-segment ID
     */
    virtual void request(const DataSegId& segId) =0;

    /**
     * Receives a notice about the remote P2P-server. Passes the information to the P2P manager.
     * @param[in] srvrInfo  Information on the remote P2P-server
     * @see P2pMgr::recvNotice(const P2pSrvrInfo&, SockAddr)
     */
    virtual void recvNotice(const P2pSrvrInfo& srvrInfo) =0;
    /**
     * Receives a notice about available product-information.
     *
     * @param[in] prodId   Identifier of the data-product
     * @retval    true     Request the datum
     * @retval    false    Don't request the datum
     */
    virtual bool recvNotice(const ProdId    prodId) =0;
    /**
     * Receives a notice about an available data-segment.
     *
     * @param[in] dataSegId  Identifier of the data-segment
     * @retval    true       Request the datum
     * @retval    false      Don't request the datum
     */
    virtual bool recvNotice(const DataSegId dataSegId) =0;

    /**
     * Receives a request for product-information.
     *
     * @param[in] prodId   Identifier of the data-product
     */
    virtual void recvRequest(const ProdId    prodId) =0;
    /**
     * Receives a request for a data-segment.
     *
     * @param[in] dataSegId  Identifier of the data-segment
     */
    virtual void recvRequest(const DataSegId dataSegId) =0;

    /**
     * Receives information on a product.
     *
     * @param[in] prodInfo  Data-product information
     * @retval    true      Success
     * @retval    false     EOF
     */
    virtual void recvData(const ProdInfo prodInfo) =0;
    /**
     * Receives a data-segment.
     *
     * @param[in] dataSeg  Data-segment
     * @retval    true     Success
     * @retval    false    EOF
     */
    virtual void recvData(const DataSeg dataSeg) =0;

    /**
     * Drains outstanding requests to the subscriber's P2P manager. Should only be called after the
     * peer has stopped.
     *
     * @throw LogicError  This instance is for a publisher and doesn't request data
     * @see `SubP2pMgr::missed()`
     */
    virtual void drainPending() =0;
};

/******************************************************************************/

template<typename P2P_MGR> class P2pSrvr; ///< Forward declaration
/// Smart pointer to an implementation
template<typename P2P_MGR> using P2pSrvrPtr = std::shared_ptr<P2pSrvr<P2P_MGR>>;
using PubP2pSrvrPtr = P2pSrvrPtr<PubP2pMgr>; ///< Publisher's P2P-server smart pointer
using SubP2pSrvrPtr = P2pSrvrPtr<SubP2pMgr>; ///< Subscriber's P2P-server smart pointer

/**
 * Interface for a P2P-server. A P2P-server creates server-side peers by accepting connections
 * initiated remotely.
 *
 * @tparam P2P_MGR  Type of P2P manager: `PubP2pMgr` or `SubP2pMgr`
 */
template<typename P2P_MGR>
class P2pSrvr
{
public:
    /**
     * Returns a smart pointer to a new instance.
     * @param[in] srvrAddr     Socket address for the server to use. Must not be the wildcard. A
     *                         port number of 0 obtains a system chosen one.
     * @param[in] maxPendConn  Maximum number of pending connections
     * @throw InvalidArgument  Maximum number of pending connections size is zero
     */
    static P2pSrvrPtr<P2P_MGR> create(
            const SockAddr srvrAddr,
            const unsigned maxPendConn);

    /**
     * Returns a smart pointer to an instance.
     * @param[in] peerConnSrvr  Peer-connection server
     */
    static P2pSrvrPtr<P2P_MGR> create(const PeerConnSrvrPtr peerConnSrvr);

    virtual ~P2pSrvr() {};

    /**
     * Returns the socket address of the P2P-server.
     * @return The socket address of the P2P-server
     */
    virtual SockAddr getSrvrAddr() const =0;


    /**
     * Returns a string representation of this instance.
     * @return A string representation of this instance
     */
    virtual String to_string() const =0;

    /**
     * Returns the next, accepted peer. Will test false if `halt()` has been called.
     * @param p2pMgr
     * @return
     */
    virtual PeerPtr accept(P2P_MGR& p2pMgr) =0;

    /**
     * Causes `accept()` to return a false object.
     * @see accept()
     */
    virtual void halt() =0;
};

using PubP2pSrvr = P2pSrvr<PubP2pMgr>; ///< Type of publisher's P2P-server
using SubP2pSrvr = P2pSrvr<SubP2pMgr>; ///< Type of subscriber's P2P-server

} // namespace

/******************************************************************************/

namespace std {
    /// Hash code class-function for an implementation of a peer
    template<>
    struct hash<hycast::PeerPtr> {
        /**
         * Returns the hash code of a peer.
         * @param[in] peer  The peer
         * @return The hash code of the peer
         */
        size_t operator()(const hycast::PeerPtr peer) const noexcept {
            return peer->hash();
        }
    };

    /// Less-than class function for an implementation of a peer
    template<>
    struct less<hycast::PeerPtr> {
        /**
         * Indicates if one peer is less than another
         * @param[in] rhs       The left-hand-side peer
         * @param[in] lhs       The right-hand-side peer
         * @retval    true      The first peer is less than the second
         * @retval    false     The first peer is not less than the second
         */
        bool operator()(
                const hycast::PeerPtr lhs,
                const hycast::PeerPtr rhs) const noexcept {
            return *lhs < *rhs;
        }
    };
}

#endif /* MAIN_PROTO_PEER_H_ */
