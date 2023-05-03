/**
 * This file declares the Peer class. The Peer class handles low-level,
 * bidirectional messaging with its remote counterpart.
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

/**
 * Interface for handling low-level, asynchronous, bidirectional messaging with a remote
 * counterpart.
 */
class Peer
{
public:
    /// Smart pointer to the implementation
    using Pimpl = std::shared_ptr<Peer>;

    /**
     * Publisher's server-side construction.
     *
     * @param[in] p2pMgr  Publisher's P2P manager
     * @param[in] conn    Connection with remote peer
     */
    static Pimpl create(
            PubP2pMgr&      p2pMgr,
            PeerConn::Pimpl conn);

    /**
     * Subscriber's client- & server-side construction.
     *
     * @param[in] p2pMgr  Subscriber's P2P manager
     * @param[in] conn    Connection with remote peer
    static Pimpl create(
            SubP2pMgr&      p2pMgr,
            PeerConn::Pimpl conn);
     */

    /**
     * Subscriber's client-side construction. The resulting peer is fully connected and ready for
     * `start()` to be called.
     *
     * @param[in] p2pMgr        Subscriber's P2P manager
     * @param[in] srvrAddr      Address of remote P2P-server
     * @throw     LogicError    Destination port number is zero
     * @throw     SystemError   Couldn't connect. Bad failure.
     * @throw     RuntimeError  Couldn't connect. Might be temporary.
     * @see       `start()`
     */
    static Pimpl create(
            SubP2pMgr&     p2pMgr,
            const SockAddr srvrAddr);

    virtual ~Peer()
    {}

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
     * Indicates if the remote peer is a publisher (and not a subscriber).
     * @retval true   The remote peer is a publisher
     * @retval false  The remote peer is not a publisher
     */
    virtual bool isRmtPub() const noexcept =0;

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
     * Executes this instance. Doesn't return until
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
     * Notifies the remote peer as to whether the local node has a path to the publisher.
     *
     * @param[in] havePubPath  Does the local node have a path to the publisher?
     */
    virtual void notifyHavePubPath(const bool havePubPath) =0;

    /**
     * Tells the remote peer to add a potential P2P-server.
     *
     * @param[in] srvrInfo  Information on the P2P-server to be added
     */
    virtual void add(const P2pSrvrInfo& srvrInfo) =0;
    /**
     * Tells the remote peer to add some potential P2P-servers.
     *
     * @param[in] tracker  P2P-servers to add
     */
    virtual void add(const Tracker& tracker) =0;

    /**
     * Notifies the remote peer about available data.
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
     * Receives notification to add a potential P2P-server.
     *
     * @param[in] srvrInfo  Potential P2P-server to add
     * @retval    true      Success
     * @retval    false     EOF
     */
    virtual void recvAdd(const P2pSrvrInfo& srvrInfo) =0;
    /**
     * Receives notification to add potential P2P-servers.
     *
     * @param[in] tracker  Potential P2P-servers to add
     * @retval    true     Success
     * @retval    false    EOF
     */
    virtual void recvAdd(const Tracker tracker) =0;

    /**
     * Receives notification of available product-information.
     *
     * @param[in] prodId   Identifier of the data-product
     * @retval    true     Request the datum
     * @retval    false    Don't request the datum
     */
    virtual bool recvNotice(const ProdId    prodId) =0;
    /**
     * Receives notification of an available data-segment.
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
     * @throw LogicError  This instance doesn't request data
     * @see `SubP2pMgr::missed()`
     */
    virtual void drainPending() =0;
};

/******************************************************************************/

/**
 * Interface for a P2P-server.
 *
 * @tparam P2P_MGR  Type of P2P manager: `PubP2pMgr` or `SubP2pMgr`
 */
template<typename P2P_MGR>
class P2pSrvr
{
public:
    /// Smart pointer to the implementation
    using Pimpl = std::shared_ptr<P2pSrvr<P2P_MGR>>;

    /**
     * Returns a smart pointer to an instance.
     * @param[in] srvrAddr     Socket address for the server to use. Must not be the wildcard. A
     *                         port number of 0 obtains a system chosen one.
     * @param[in] maxPendConn  Maximum number of pending connections
     * @throw InvalidArgument  Accept-queue size is zero
     */
    static Pimpl create(
            const SockAddr srvrAddr,
            const unsigned maxPendConn);

    /**
     * Returns a smart pointer to an instance.
     * @param[in] peerConnSrvr  Peer-connection server
     */
    static Pimpl create(const PeerConnSrvr::Pimpl peerConnSrvr);

    virtual ~P2pSrvr() {};

    /**
     * Returns the socket address of the P2P-server.
     * @return The socket address of the P2P-server
     */
    virtual SockAddr getSrvrAddr() const =0;

    /**
     * Returns the next, accepted peer. Will test false if `halt()` has been called.
     * @param p2pMgr
     * @return
     */
    virtual Peer::Pimpl accept(P2P_MGR& p2pMgr) =0;

    /**
     * Causes `accept()` to return a false object.
     * @see accept()
     */
    virtual void halt() =0;
};

using PubPeerSrvr = P2pSrvr<PubP2pMgr>; ///< Type of publisher's P2P-server
using SubPeerSrvr = P2pSrvr<SubP2pMgr>; ///< Type of subscriber's P2P-server

} // namespace

/******************************************************************************/

namespace std {
    /// Hash code class-function for an implementation of a peer
    template<>
    struct hash<hycast::Peer::Pimpl> {
        /**
         * Returns the hash code of a peer.
         * @param[in] peer  The peer
         * @return The hash code of the peer
         */
        size_t operator()(const hycast::Peer::Pimpl peer) const noexcept {
            return peer->hash();
        }
    };

    /// Less-than class function for an implementation of a peer
    template<>
    struct less<hycast::Peer::Pimpl> {
        /**
         * Indicates if one peer is less than another
         * @param[in] rhs       The left-hand-side peer
         * @param[in] lhs       The right-hand-side peer
         * @retval    true      The first peer is less than the second
         * @retval    false     The first peer is not less than the second
         */
        bool operator()(
                const hycast::Peer::Pimpl lhs,
                const hycast::Peer::Pimpl rhs) const noexcept {
            return *lhs < *rhs;
        }
    };
}

#endif /* MAIN_PROTO_PEER_H_ */
