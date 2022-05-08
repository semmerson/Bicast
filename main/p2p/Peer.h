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
     * Server-side construction.
     *
     * @param[in] p2pMgr  Publisher's P2P manager
     * @param[in] rpc     RPC instance
     */
    static Pimpl create(
            PubP2pMgr& p2pMgr,
            Rpc::Pimpl rpc);

    /**
     * Server-side construction.
     *
     * @param[in] p2pMgr       Subscriber's P2P manager
     * @param[in] rpc          RPC instance
     * @param[in] dataSock     Socket for data
     */
    static Pimpl create(
            SubP2pMgr& p2pMgr,
            Rpc::Pimpl rpc);

    /**
     * Client-side construction. The resulting peer is fully connected and ready
     * for `start()` to be called.
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
     * Returns the socket address of the local peer.
     *
     * @return Socket address of local peer
     */
    virtual SockAddr getLclAddr() const noexcept =0;

    /**
     * Returns the socket address of the remote peer.
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
     * Notifies the remote peer.
     *
     * @retval    `true`      Success
     * @retval    `false`     Connection lost
     * @throw     LogicError  Remote peer is publisher
     * @throw     LogicError  `start()` not yet called
     * @see       `start()`
     */
    virtual bool notify(const Tracker   tracker) =0;
    virtual bool notify(const SockAddr  srvrAddr) =0;
    virtual bool notify(const ProdId    prodId) =0;
    virtual bool notify(const DataSegId dataSegId) =0;

    virtual bool recvNotice(const ProdId    prodId) =0;
    virtual bool recvNotice(const DataSegId dataSegId) =0;

    virtual bool recvRequest(const ProdId    prodId) =0;
    virtual bool recvRequest(const DataSegId dataSegId) =0;

    virtual void recvData(const Tracker tracker) =0;
    virtual void recvData(const SockAddr srvrAddr) =0;
    virtual void recvData(const ProdInfo prodInfo) =0;
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
     * @throw InvalidArgument  Backlog argument is zero
     */
    static Pimpl create(
            const SockAddr srvrAddr,
            const unsigned backlog = 8);

    virtual ~PeerSrvr() {};

    virtual SockAddr getSrvrAddr() const =0;

    virtual Peer::Pimpl accept(P2P_MGR& p2pMgr) =0;
};

using PubPeerSrvr = PeerSrvr<PubP2pMgr>;
using SubPeerSrvr = PeerSrvr<SubP2pMgr>;

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
