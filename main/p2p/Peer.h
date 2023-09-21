/**
 * This file declares the Peer class. A peer handles low-level, asynchronous, bidirectional
 * messaging with its remote counterpart.
 *
 *  @file:  Peer.h
 * @author: Steven R. Emmerson <emmerson@ucar.edu>
 *
 *    Copyright 2023 University Corporation for Atmospheric Research
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
    /// Common interface for a client of a Peer (both publishing and subscribing clients).
    class BaseMgr
    {
    public:
        /**
         * Destroys.
         */
        virtual ~BaseMgr() noexcept =default;

        /**
         * Returns information on the P2P-server.
         *
         * @return  Information on the 's P2P-server
         */
        virtual P2pSrvrInfo getSrvrInfo() =0;

        /**
         * Receives information on P2P-servers.
         * @param[in] tracker  Information on P2P-servers
         */
        virtual void recv(const Tracker& tracker) =0;

        /**
         * Receives information on a P2P-server.
         * @param[in] srvrInfo  Information on the P2P-server
         */
        virtual void recvNotice(const P2pSrvrInfo& srvrInfo) =0;

        /**
         * Returns a set of this instance's identifiers of complete products minus those of another
         * set.
         *
         * @param[in]  rhs      Other set of product identifiers to be subtracted from the ones this
         *                      instance has
         * @return              This instance's identifiers minus those of the other set
         */
        virtual ProdIdSet subtract(ProdIdSet rhs) const =0;

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

    /// Interface for a publishing client of a peer
    using PubMgr = BaseMgr;

    /// Interface for a subscribing client of a Peer.
    class SubMgr : virtual public BaseMgr
    {
    public:
        /**
         * Destroys.
         */
        virtual ~SubMgr() noexcept =default;

        /**
         * Returns the set of identifiers of complete products.
         *
         * @return             Set of complete product identifiers
         */
        virtual ProdIdSet getProdIds() const =0;

        /**
         * Receives a notice of available product information from a remote peer.
         *
         * @param[in] notice       Which product
         * @param[in] rmtAddr      Socket address of remote peer
         * @retval    false        Local peer shouldn't request from remote peer
         * @retval    true         Local peer should request from remote peer
         */
        virtual bool recvNotice(
                const ProdId   notice,
                const SockAddr rmtAddr) =0;

        /**
         * Receives a notice of an available data-segment from a remote peer.
         *
         * @param[in] notice       Which data-segment
         * @param[in] rmtAddr      Socket address of remote peer
         * @retval    false        Local peer shouldn't request from remote peer
         * @retval    true         Local peer should request from remote peer
         */
        virtual bool recvNotice(
                const DataSegId notice,
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
        virtual void missed(
                const DataSegId dataSegId,
                SockAddr        rmtAddr) =0;

        /**
         * Receives product information from a remote peer.
         *
         * @param[in] prodInfo  Product information
         * @param[in] rmtAddr   Socket address of remote peer
         */
        virtual void recvData(
                const ProdInfo prodInfo,
                SockAddr       rmtAddr) =0;

        /**
         * Receives a data segment from a remote peer.
         *
         * @param[in] dataSeg  Data segment
         * @param[in] rmtAddr  Socket address of remote peer
         */
        virtual void recvData(
                const DataSeg dataSeg,
                SockAddr      rmtAddr) =0;
    };

    /**
     * Returns a publisher's server-side implementation. The resulting peer is fully connected and
     * ready for `run()` to be called.
     *
     * @param[in] pubPeerMgr       Associated publisher's peer manager
     * @param[in] conn             Connection with remote peer
     * @see       run()
     */
    static PeerPtr create(
            PubMgr&     pubPeerMgr,
            PeerConnPtr conn);

    /**
     * Returns a subscriber's server-side implementation. The resulting peer is fully connected and
     * ready for `run()` to be called.
     *
     * @param[in] subPeerMgr       Associated subscriber's peer manager
     * @param[in] conn             Connection with remote peer
     * @see       run()
     */
    static PeerPtr create(
            SubMgr&      subPeerMgr,
            PeerConnPtr& conn);

    /**
     * Returns a subscriber's client-side implementation. The resulting peer is fully connected and
     * ready for `run()` to be called.
     *
     * @param[in] subPeerMgr       Associated subscriber's peer manager
     * @param[in] srvrAddr         Address of remote P2P-server
     * @throw     LogicError       Destination port number is zero
     * @throw     SystemError      Couldn't connect. Bad failure.
     * @throw     RuntimeError     Couldn't connect. Might be temporary.
     * @see       run()
     */
    static PeerPtr create(
            SubMgr&         subPeerMgr,
            const SockAddr& srvrAddr);

    virtual ~Peer() noexcept =default;

    /**
     * Returns a reference to the underlying connection with the remote peer.
     * @return A reference to the underlying connection with the remote peer
     */
    virtual PeerConnPtr& getConnection() =0;

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
     * construction. Called by the peer manager.
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
    virtual const SockAddr& getLclAddr() const noexcept =0;

    /**
     * Returns the socket address of the remote peer. This will be the socket address of the remote
     * P2P-server iff this peer was constructed client-side (i.e., initiated the connection).
     *
     * @return Socket address of remote peer
     */
    virtual const SockAddr& getRmtAddr() const noexcept =0;

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
     * Receives a notice about the remote P2P-server. Passes the information to the peer manager.
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
     */
    virtual void recvData(const ProdInfo prodInfo) =0;
    /**
     * Receives a data-segment.
     *
     * @param[in] dataSeg  Data-segment
     */
    virtual void recvData(const DataSeg dataSeg) =0;

    /**
     * Drains outstanding requests to the subscriber's peer manager. Should only be called after the
     * peer has stopped.
     *
     * @throw LogicError  This instance is for a publisher and doesn't request data
     * @see `SubP2pMgr::missed()`
     */
    virtual void drainPending() =0;
};

/******************************************************************************/

template<typename PEER_MGR> class P2pSrvr; ///< Forward declaration
/// Smart pointer to an implementation
template<typename PEER_MGR> using P2pSrvrPtr = std::shared_ptr<P2pSrvr<PEER_MGR>>;
using PubP2pSrvrPtr = P2pSrvrPtr<Peer::PubMgr>; ///< Publisher's P2P-server smart pointer
using SubP2pSrvrPtr = P2pSrvrPtr<Peer::SubMgr>; ///< Subscriber's P2P-server smart pointer

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
     * @param[in] p2pMgr  Associated P2P manager
     * @return            Pointer to the accepted peer. Will test false if `halt()` has been called.
     * @see halt()
     */
    virtual PeerPtr accept(P2P_MGR& p2pMgr) =0;

    /**
     * Causes `accept()` to return a false object.
     * @see accept()
     */
    virtual void halt() =0;
};

using PubP2pSrvr = P2pSrvr<Peer::PubMgr>; ///< Type of publisher's P2P-server
using SubP2pSrvr = P2pSrvr<Peer::SubMgr>; ///< Type of subscriber's P2P-server

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
