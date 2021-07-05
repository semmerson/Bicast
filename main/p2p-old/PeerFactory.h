/**
 * Factory for creating `Peer`s
 *
 *        File: PeerFactory.h
 *  Created on: May 10, 2019
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

#ifndef MAIN_PEER_PEERFACTORY_H_
#define MAIN_PEER_PEERFACTORY_H_

#include "PortPool.h"
#include "Peer.h"
#include "SockAddr.h"

#include <memory>

namespace hycast {

/**
 * Abstract base class for creating peers.
 */
class PeerFactory
{
protected:
    class Impl;

    std::shared_ptr<Impl> pImpl;

    /**
     * Default constructs.
     */
    PeerFactory() =default;

    PeerFactory(Impl* impl);

public:
    /**
     * Destroys.
     */
    virtual ~PeerFactory() noexcept =default;

    SockAddr getSrvrAddr() const;

    /**
     * Returns the port number of the server's socket in host byte-order.
     *
     * @return Port number of server's socket in host byte-order
     */
    in_port_t getPort() const;

    /**
     * Closes the factory. Causes any outstanding and subsequent calls to
     * `accept()` to return a default-constructed peer. Idempotent.
     *
     * @throws std::system_error  System failure
     */
    void close();
};

/**
 * Factory for creating publisher-peers.
 */
class PubPeerFactory final : public PeerFactory
{
    class Impl;

public:
    /**
     * Default constructs.
     */
    PubPeerFactory();

    /**
     * Constructs. Creates a server that listens on the given, local socket
     * address.  Calls `::listen()`.
     *
     * @param[in] srvrAddr      Socket address on which a local server will
     *                          accept connections from remote peers
     * @param[in] queueSize     Size of server's `listen()` queue
     * @param[in] peerMgr       Peer manager
     */
    PubPeerFactory(
            const SockAddr& srvrAddr,
            const int       queueSize,
            SendPeerMgr&    peerMgr);

    /**
     * Accepts a connection from a remote peer. `Peer::operator()` has not been
     * called on the returned instance. Blocks until a connection is accepted or
     * an exception is thrown.
     *
     * @return                 Local peer. Will test false if `close()` has been
     *                         called.
     * @cancellationpoint      Yes
     */
    Peer accept();
};

/**
 * Factory for creating subscriber-peers.
 */
class SubPeerFactory final : public PeerFactory
{
    class Impl;

public:
    /**
     * Default constructs.
     */
    SubPeerFactory();

    /**
     * Constructs. Creates a server that listens on the given, local socket
     * address.  Calls `::listen()`.
     *
     * @param[in] srvrAddr      Socket address on which a local server will
     *                          accept connections from remote peers
     * @param[in] queueSize     Size of server's `listen()` queue
     * @param[in] peerObs       Observer of the peer
     */
    SubPeerFactory(
            const SockAddr& srvrAddr,
            const int       queueSize,
            XcvrPeerMgr&    peerObs);

    /**
     * Accepts a connection from a remote peer. `Peer::operator()` has not been
     * called on the returned instance. Blocks until a connection is accpted or
     * an exception is thrown.
     *
     * @param[in] lclNodeType  Current type of local node
     * @return                 Corresponding local peer. Will test false if
     *                         `close()` has been called.
     * @cancellationpoint      Yes
     */
    Peer accept(NodeType lclNodeType);

    /**
     * Creates a local peer by connecting to a remote server. `Peer::operator()`
     * has not been called on the returned instance. Blocks until a connection
     * is established or an exception is thrown.
     *
     * @param[in] rmtAddr             Socket address of the remote server
     * @param[in] lclNodeType         Current type of local node
     * @return                        Local peer that's connected to a remote
     *                                counterpart
     * @throws    std::system_error   System error
     * @throws    std::runtime_error  Remote peer closed the connection
     * @cancellationpoint             Yes
     */
    Peer connect(
            const SockAddr& rmtAddr,
            const NodeType  lclNodeType);
};

} // namespace

#endif /* MAIN_PEER_PEERFACTORY_H_ */
