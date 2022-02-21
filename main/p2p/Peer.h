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

#if 0
template<class MGR, class PEER> class PeerSrvr;
#endif

/**
 * Abstract base class for handling low-level, asynchronous, bidirectional
 * messaging with a remote counterpart.
 */
class Peer
{
public:
    class     Impl;

protected:
    using Pimpl = std::shared_ptr<Impl>;
    Pimpl pImpl;

    Peer(Impl* impl);

public:
    /**
     * Default constructs.
     */
    Peer() =default;

    virtual ~Peer()
    {}

    /**
     * Indicates if this instance was constructed as a client.
     *
     * @retval `true`   Constructed as a client
     * @retval `false`  Constructed by a server
     */
    bool isClient() const noexcept;

    /**
     * Indicates if the remote peer belongs to the publisher.
     *
     * @retval `true`   Remote peer belongs to publisher
     * @retval `false`  Remote peer does not belong to publisher
     */
    bool isRmtPub() const noexcept;

    /**
     * Returns the socket address of the remote peer.
     *
     * @return Socket address of remote peer
     */
    SockAddr getRmtAddr() noexcept;

    virtual void start() const;

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
    void stop() const;

    /**
     * Indicates if this instance is valid (i.e., wasn't default constructed).
     *
     * @retval `false`  Instance is invalid
     * @retval `true`   Instance is valid
     */
    operator bool() const noexcept;

    size_t hash() const noexcept;

    bool operator<(const Peer& rhs) const noexcept;

    bool operator==(const Peer& rhs) const noexcept;

    bool operator!=(const Peer& rhs) const noexcept;

    /**
     * Returns the string representation of this instance. Currently, it's the
     * string representation of the socket address of the remote peer.
     *
     * @return String representation of this instance
     */
    String to_string() const;

    /**
     * Notifies the remote peer.
     *
     * @retval    `true`      Success
     * @retval    `false`     Failure
     * @throw     LogicError  Remote peer is publisher's
     * @throw     LogicError  Instance isn't in started state
     * @see       `start()`
     */
    bool notify(const Tracker   tracker) const;
    bool notify(const SockAddr  srvrAddr) const;
    bool notify(const ProdIndex prodIndex) const;
    bool notify(const DataSegId dataSegId) const;

    void recvRequest(const ProdIndex prodIndex) const;
    void recvRequest(const DataSegId dataSegId) const;

    bool rmtIsPubPath() const noexcept;
};

/**
 * A publisher's peer. Such peers are *always* server-side constructed by a
 * peer-server.
 */
class PubPeer final : public Peer
{
    class Impl;

public:
    using RpcType     = PubRpc;
    using RpcSrvrType = PubRpcSrvr;

    /**
     * Server-side construction.
     *
     * @param[in] p2pMgr  Publisher's P2P manager
     * @param[in] rpc     RPC instance
     */
    PubPeer(P2pMgr& p2pMgr, PubRpc::Pimpl rpc);

    /**
     * Default constructs. The resulting instance will test false.
     */
    PubPeer() =default;

    /**
     * Starts this instance. Creates threads on which
     *   - The sockets are read; and
     *   - The P2P manager is called.
     *
     * @throw LogicError   Already called
     * @throw LogicError   Remote peer uses unsupported protocol
     * @throw SystemError  Thread couldn't be created
     * @see `stop()`
     */
    void start() const override;

    bool notify(const Tracker notice) const;
};

/**
 * A subscriber's peer. Such peers can be server-side constructed by a
 * peer-server or client-side constructed by initiating a connection to an
 * RPC-server.
 */
class SubPeer final : public Peer
{
#if 0
    friend class PeerSrvr<SubP2pMgr, SubPeer>;
#endif

    class Impl;

public:
    using RpcType     = SubRpc;
    using RpcSrvrType = SubRpcSrvr;

    /**
     * Default constructs. The resulting instance will test false.
     */
    SubPeer() =default;

    /**
     * Server-side construction.
     *
     * @param[in] p2pMgr       Subscriber's associated P2P manager
     * @param[in] rpc          RPC instance
     * @param[in] dataSock     Socket for data
     */
    SubPeer(SubP2pMgr& p2pMgr, SubRpc::Pimpl rpc);

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
    SubPeer(SubP2pMgr&      p2pMgr,
            const SockAddr& srvrAddr);

    /**
     * Starts this instance. Creates threads on which
     *   - The sockets are read; and
     *   - The P2P manager is called.
     *
     * @throw LogicError   Already called
     * @throw LogicError   Remote peer uses unsupported protocol
     * @throw SystemError  Thread couldn't be created
     * @see `stop()`
     */
    void start() const override;

    void recvNotice(const ProdIndex prodIndex) const;

    void recvNotice(const DataSegId dataSegId) const;

    void recvData(const Tracker tracker) const;

    void recvData(const SockAddr srvrAddr) const;

    void recvData(const ProdInfo prodInfo) const;

    void recvData(const DataSeg dataSeg) const;
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

    static Pimpl create(
            const SockAddr          srvrAddr,
            const unsigned          backlog = 8);

    virtual ~PeerSrvr() {};

    virtual SockAddr getSrvrAddr() const =0;

    virtual typename P2P_MGR::PeerType accept(P2P_MGR& p2pMgr) const =0;
};

using PubPeerSrvr = PeerSrvr<P2pMgr>;
using SubPeerSrvr = PeerSrvr<SubP2pMgr>;

} // namespace

/******************************************************************************/

namespace std {
    template<>
    struct hash<hycast::Peer> {
        size_t operator()(const hycast::Peer& peer) const noexcept {
            return peer.hash();
        }
    };
}

#endif /* MAIN_PROTO_PEER_H_ */
