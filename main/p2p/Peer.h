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
#include "P2pNode.h"
#include "PeerSrvrAddrs.h"
#include "Socket.h"

#include <memory>

namespace hycast {

template<class NODE, class PEER> class PeerSrvr;

/**
 * Abstract base class for handling low-level, asynchronous, bidirectional
 * messaging with a remote counterpart.
 */
class Peer
{
public:
    class     Impl;

protected:
    using     SharedPtr = std::shared_ptr<Impl>;
    SharedPtr pImpl;

    Peer(Impl* impl);

    Peer(SharedPtr& pImpl);

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
     * Starts this instance. Creates threads that serve the remote peer.
     * Upon return, this instance *must* be stopped before it can be destroyed.
     *
     * @retval `false`     Remote peer disconnected
     * @retval `true`      Success
     * @throw LogicError   Already started
     * @throw SystemError  Thread couldn't be created
     * @see   `stop()`
     */
    bool start();

    /**
     * Returns the socket address of the remote peer.
     *
     * @return Socket address of remote peer
     */
    SockAddr getRmtAddr() noexcept;

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
    void stop();

    /**
     * Indicates if this instance is valid (i.e., wasn't default constructed).
     *
     * @retval `false`  Instance is invalid
     * @retval `true`   Instance is valid
     */
    operator bool() const;

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
     * @retval    `false`     No connection. Connection was lost or `start()`
     *                        wasn't called.
     * @retval    `true`      Success
     * @throw     LogicError  Instance isn't in started state
     * @see       `start()`
     */
    bool notify(const PubPath notice) const;
    bool notify(const PeerSrvrAddrs notice) const;
    bool notify(const ProdIndex notice) const;
    bool notify(const DataSegId& notice) const;

    bool rmtIsPubPath() const noexcept;
};

/// A publisher's peer. Such peers are *always* constructed by a peer-server.
class PubPeer final : public Peer
{
    class     Impl;

public:
    /**
     * Server-side construction.
     *
     * @param[in] node         Subscriber's associated P2P node
     * @param[in] socks        Sockets in order: notice, request, data
     * @param[in] requestSock  Socket for requests
     * @param[in] dataSock     Socket for data
     */
    PubPeer(P2pNode& node, TcpSock socks[3]);

    /**
     * Default constructs. The resulting instance will test false.
     */
    PubPeer() =default;
};

/// A subscriber's peer
class SubPeer final : public Peer
{
    friend class PeerSrvr<SubP2pNode, SubPeer>;

    class     Impl;

    /**
     * Server-side construction.
     *
     * @param[in] node         Subscriber's associated P2P node
     * @param[in] socks        Sockets in order: notice, request, data
     * @param[in] dataSock     Socket for data
     */
    SubPeer(SubP2pNode& node, TcpSock socks[3]);

public:
    /**
     * Default constructs. The resulting instance will test false.
     */
    SubPeer() =default;

    /**
     * Client-side construction.
     *
     * @param[in] node         P2P node to connect to
     * @param[in] srvrAddr     Address of server to which to connect
     * @param[in] isPublisher  Remote peer-server is publisher's
     */
    SubPeer(SubP2pNode&      node,
            const SockAddr&  srvrAddr,
            const bool       isPublisher);
};

/**
 * Peer server. Listens for incoming connections from remote peer clients.
 *
 * @tparam NODE  Type of P2P node (`P2pNode` or `SubP2pNode`)
 * @tparam PEER  Type of peer (`PubPeer` or `SubPeer`)
 */
template<class NODE, class PEER>
class PeerSrvr
{
    class Impl;

    std::shared_ptr<Impl> pImpl;

public:
    PeerSrvr() =default;

    /**
     * Constructs.
     *
     * @param[in] node       P2P node
     * @param[in] srvrAddr   Address of interface for peer server. Must not
     *                       specify any interface.
     * @param[in] maxAccept  Maximum number of outstanding peer connections
     */
    PeerSrvr(NODE&          node,
             const SockAddr srvrAddr,
             const unsigned maxAccept = 8);

    operator bool() {
        return pImpl->operator bool();
    }

    SockAddr getSockAddr() const;

    /**
     * Returns the next peer. Blocks until one is available.
     *
     * @return  The next peer
     * @throws  SystemError  `::accept()` failure
     */
    PEER accept();
};

using PubPeerSrvr = PeerSrvr<P2pNode, PubPeer>;
using SubPeerSrvr = PeerSrvr<SubP2pNode, SubPeer>;

} // namespace

namespace std {
    template<>
    struct hash<hycast::Peer> {
        size_t operator()(const hycast::Peer& peer) const noexcept {
            return peer.hash();
        }
    };
}

#endif /* MAIN_PROTO_PEER_H_ */
