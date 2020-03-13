/**
 * Factory for creating `Peer`s
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: PeerFactory.h
 *  Created on: May 10, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_PEER_PEERFACTORY_H_
#define MAIN_PEER_PEERFACTORY_H_

#include <PortPool.h>
#include "Peer.h"
#include "SockAddr.h"

#include <memory>

namespace hycast {

class PeerFactory
{
public:
    class Impl;

protected:
    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Default constructs.
     */
    PeerFactory();

    /**
     * Constructs. Creates a server that listens on the given, local socket
     * address.  Calls `::listen()`.
     *
     * @param[in] srvrAddr   Socket address on which a local server will accept
     *                       connections from remote peers
     * @param[in] queueSize  Size of server's `listen()` queue
     * @param[in] portPool   Pool of available port numbers
     * @param[in] peerObs    Observer of the remote peer
     * @param[in] nodeType   Type of node
     */
    PeerFactory(
            const SockAddr& srvrAddr,
            const int       queueSize,
            PortPool&       portPool,
            PeerObs&        peerObs,
            NodeType&       nodeType);

    /**
     * Returns the port number of the server's socket in host byte-order.
     *
     * @return Port number of server's socket in host byte-order
     */
    in_port_t getPort() const;

    /**
     * Accepts a connection from a remote peer. `Peer::operator()` has not been
     * called on the returned instance. Potentially slow.
     *
     * @return             Corresponding local peer. Will test false if
     *                     `close()` has been called.
     * @cancellationpoint  Yes
     */
    Peer accept();

    /**
     * Creates a local peer by connecting to a remote server. `Peer::operator()`
     * has not been called on the returned instance.
     *
     * @return                        Local peer that's connected to a remote
     *                                counterpart
     * @param[in] rmtAddr             Socket address of the remote server
     * @throws    std::system_error   System error
     * @throws    std::runtime_error  Remote peer closed the connection
     * @cancellationpoint             Yes
     */
    Peer connect(const SockAddr& rmtAddr);

    /**
     * Closes the factory. Causes any outstanding and subsequent calls to
     * `accept()` to return a default-constructed peer. Idempotent.
     *
     * @throws std::system_error  System failure
     */
    void close();
};

/******************************************************************************/

/**
 * Peer factory that creates peers with three `Wire`s to minimize latency.
 */
class PeerFactory3 : public PeerFactory
{
protected:
    class Impl;

public:
    /**
     * Constructs.
     *
     * @param[in] lclAddr  Address for local server that accepts connections
     *                     from remote peers
     */
    PeerFactory3(const SockAddr& lclAddr);
};

} // namespace

#endif /* MAIN_PEER_PEERFACTORY_H_ */
