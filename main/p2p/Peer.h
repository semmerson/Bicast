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
#include "Rpc.h"
#include "Socket.h"
#include "Tracker.h"

#include <memory>

namespace hycast {

/**
 * Interface for handling low-level, asynchronous, bidirectional messaging with
 * a remote counterpart.
 */
class Peer
{
public:
    using Pimpl = std::shared_ptr<Peer>;

    /**
     * Publisher's server-side construction.
     *
     * @param[in] p2pMgr  Publisher's P2P manager
     * @param[in] rpc     RPC instance
     */
    static Pimpl create(
            PubP2pMgr& p2pMgr,
            Rpc::Pimpl rpc);

    /**
     * Subscriber's client- & server-side construction.
     *
     * @param[in] p2pMgr       Subscriber's P2P manager
     * @param[in] rpc          RPC instance
     * @param[in] dataSock     Socket for data
     */
    static Pimpl create(
            SubP2pMgr& p2pMgr,
            Rpc::Pimpl rpc);

    /**
     * Subscriber's client-side construction. The resulting peer is fully connected and ready for
     * `start()` to be called.
     *
     * @param[in] p2pMgr        Subscriber's P2P manager
     * @param[in] srvrAddr      Address of remote peer-server
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
     * Indicates if this instance was constructed as a client.
     *
     * @retval `true`   Constructed as a client
     * @retval `false`  Constructed by a server
     */
    virtual bool isClient() const noexcept =0;

    /**
     * Indicates if this instance is the publisher.
     *
     * @retval `true`   Instance is publisher
     * @retval `false`  Instance is not publisher
     */
    virtual bool isPub() const noexcept =0;

    /**
     * Indicates if the remote peer is the publisher.
     *
     * @retval `true`   Remote peer is publisher
     * @retval `false`  Remote peer is not publisher
     */
    virtual bool isRmtPub() const noexcept =0;

    /**
     * Indicates if the remote node is a path to the publisher.
     *
     * @retval `true`   Remote node is a path to the publisher
     * @retval `false`  Remote node isn't a path to the publisher
     */
    virtual bool isRmtPathToPub() const noexcept =0;

    /**
     * Returns the socket address of the local peer.
     *
     * @return Socket address of local peer
     */
    virtual SockAddr getLclAddr() const noexcept =0;

    /**
     * Returns the socket address of the remote peer. This will be the socket address of the remote
     * P2P server iff this peer was constructed client-side (i.e., initiated the connection).
     *
     * @return Socket address of remote peer
     */
    virtual SockAddr getRmtAddr() const noexcept =0;

    virtual size_t hash() const noexcept =0;

    virtual bool operator<(const Peer& rhs) const noexcept =0;

    virtual bool operator==(const Peer& rhs) const noexcept =0;

    virtual bool operator!=(const Peer& rhs) const noexcept =0;

    /**
     * Returns the string representation of this instance. Currently, it's the
     * string representation of the socket address of the remote peer.
     *
     * @return String representation of this instance
     */
    virtual String to_string() const =0;

    /**
     * Starts serving the remote peer.
     */
    virtual void start() =0;

    /**
     * Stops this instance from serving its remote counterpart. Does the
     * following:
     *   - Stops the threads that are serving the remote peer
     *   - Joins those threads
     * *Must* be called in order for this instance to be destroyed
     *
     * @throw LogicError  Peer hasn't been started
     * @see   `start()`
     */
    virtual void stop() =0;

    /**
     * Notifies the remote peer as to whether the local node has a path to the publisher.
     *
     * @param[in] havePubPath  Does the local node have a path to the publisher?
     * @retval    `true`       Success
     * @retval    `false`      Lost connection
     */
    virtual bool notifyHavePubPath(const bool havePubPath) =0;

    /**
     * Tells the remote peer to add a potential peer-server.
     *
     * @param[in] p2pSrvr  Peer-server to add
     * @retval    `true`   Success
     * @retval    `false`  EOF
     */
    virtual bool add(const SockAddr p2pSrvr) =0;
    /**
     * Tells the remote peer to add some potential peer-servers.
     *
     * @param[in] tracker  Peer-servers to add
     * @retval    `true`   Success
     * @retval    `false`  EOF
     */
    virtual bool add(const Tracker  tracker) =0;

    /**
     * Tells the remote peer to remove a potential peer-server.
     *
     * @param[in] p2pSrvr  Peer-server to remove
     * @retval    `true`   Success
     * @retval    `false`  EOF
     */
    virtual bool remove(const SockAddr p2pSrvr) =0;
    /**
     * Tells the remote peer to remove some potential peer-servers.
     *
     * @param[in] tracker  Peer-servers to remove
     * @retval    `true`   Success
     * @retval    `false`  EOF
     */
    virtual bool remove(const Tracker tracker) =0;

    /**
     * Notifies the remote peer about available data.
     *
     * @retval    `true`      Success
     * @retval    `false`     Connection lost
     * @throw     LogicError  Remote peer is publisher
     * @throw     LogicError  `start()` not yet called
     * @see       `start()`
     */
    virtual bool notify(const ProdId    prodId) =0;
    virtual bool notify(const DataSegId dataSegId) =0;

    /**
     * Receives notification as to whether the remote node has a path to the publisher.
     *
     * @param[in] hasPubPath  Does the remote node have a path to the publisher?
     */
    virtual void recvHavePubPath(const bool hasPubPath) =0;

    /**
     * Receives the set of product-identifiers that the remote peer has for determination of the
     * backlog of products to notify the remote peer about. The remote peer will not be the
     * publisher.
     *
     * @param[in] prodIds  Product-identifiers
     * @throw LogicError   Remote peer is the publisher
     * @throw LogicError   The backlog is already being handled
     */
    virtual void recvHaveProds(ProdIdSet::Pimpl prodIds) =0;

    /**
     * Receives notification to add a potential peer-server.
     *
     * @param[in] p2pSrvr  Potential peer-server to add
     * @retval    `true`   Success
     * @retval    `false`  EOF
     */
    virtual void recvAdd(const SockAddr p2pSrvr) =0;
    /**
     * Receives notification to add potential peer-servers.
     *
     * @param[in] p2pSrvr  Potential peer-servers to add
     * @retval    `true`   Success
     * @retval    `false`  EOF
     */
    virtual void recvAdd(const Tracker tracker) =0;

    /**
     * Receives notification to remove a potential peer-server.
     *
     * @param[in] p2pSrvr  Potential peer-server to remove
     * @retval    `true`   Success
     * @retval    `false`  EOF
     */
    virtual void recvRemove(const SockAddr p2pSrvr) =0;
    /**
     * Receives notification to remove potential peer-servers.
     *
     * @param[in] p2pSrvr  Potential peer-servers to remove
     * @retval    `true`   Success
     * @retval    `false`  EOF
     */
    virtual void recvRemove(const Tracker tracker) =0;

    /**
     * Receives notification of available product-information.
     *
     * @param[in] prodId   Identifier of the data-product
     * @retval    `true`   Success
     * @retval    `false`  EOF
     */
    virtual bool recvNotice(const ProdId    prodId) =0;
    /**
     * Receives notification of an available data-segment.
     *
     * @param[in] dataSegId  Identifier of the data-segment
     * @retval    `true`     Success
     * @retval    `false`    EOF
     */
    virtual bool recvNotice(const DataSegId dataSegId) =0;

    /**
     * Receives a request for product-information.
     *
     * @param[in] prodId   Identifier of the data-product
     * @retval    `true`   Success
     * @retval    `false`  EOF
     */
    virtual bool recvRequest(const ProdId    prodId) =0;
    /**
     * Receives a request for a data-segment.
     *
     * @param[in] dataSegId  Identifier of the data-segment
     * @retval    `true`     Success
     * @retval    `false`    EOF
     */
    virtual bool recvRequest(const DataSegId dataSegId) =0;

    /**
     * Receives information on a product.
     *
     * @param[in] prodInfo  Data-product information
     * @retval    `true`    Success
     * @retval    `false`   EOF
     */
    virtual void recvData(const ProdInfo prodInfo) =0;
    /**
     * Receives a data-segment.
     *
     * @param[in] dataSeg  Data-segment
     * @retval    `true`   Success
     * @retval    `false`  EOF
     */
    virtual void recvData(const DataSeg dataSeg) =0;
};

/******************************************************************************/

/**
 * Interface for a peer-server.
 *
 * @tparam P2P_MGR  Type of P2P manager: `PubP2pMgr` or `SubP2pMgr`
 */
template<typename P2P_MGR>
class PeerSrvr
{
public:
    using Pimpl = std::shared_ptr<PeerSrvr<P2P_MGR>>;

    /**
     * @throw InvalidArgument  Accept-queue size is zero
     */
    static Pimpl create(
            const TcpSrvrSock srvrSock,
            const unsigned    acceptQSize);

    /**
     * @throw InvalidArgument  Accept-queue size is zero
     */
    static Pimpl create(
            const SockAddr srvrAddr,
            const unsigned acceptQSize);

    virtual ~PeerSrvr() {};

    virtual SockAddr getSrvrAddr() const =0;

    virtual Peer::Pimpl accept(P2P_MGR& p2pMgr) =0;
};

using PubP2pSrvr = PeerSrvr<PubP2pMgr>;
using SubP2pSrvr = PeerSrvr<SubP2pMgr>;

} // namespace

/******************************************************************************/

namespace std {
    template<>
    struct hash<hycast::Peer::Pimpl> {
        size_t operator()(const hycast::Peer::Pimpl peer) const noexcept {
            return peer->hash();
        }
    };

    template<>
    struct less<hycast::Peer::Pimpl> {
        bool operator()(
                const hycast::Peer::Pimpl peer1,
                const hycast::Peer::Pimpl peer2) const noexcept {
            return *peer1 < *peer2;
        }
    };
}

#endif /* MAIN_PROTO_PEER_H_ */
