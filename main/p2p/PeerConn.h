/**
 * Connection between peers.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: PeerConn.h
 *  Created on: May 10, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_PEER_PEERCONN_H_
#define MAIN_PEER_PEERCONN_H_

#include "Chunk.h"
#include "PeerMsgSndr.h"
#include "PortPool.h"
#include "SockAddr.h"

#include <memory>

namespace hycast {

class PeerConn
{
public:
    class                 Impl;

protected:
    std::shared_ptr<Impl> pImpl;

    /**
     * Constructs from an implementation.
     *
     * @param[in] impl  The implementation
     */
    PeerConn(Impl* const impl);

public:
    /**
     * Server-side construction.
     *
     * @param[in] sock      `::accept()`ed socket
     * @param[in] portPool  Pool of potential port numbers for temporary servers
     */
    PeerConn(
            TcpSock&  sock,
            PortPool& portPool);

    /**
     * Server-side construction.
     *
     * @param[in] sock      `::accept()`ed socket
     * @param[in] portPool  Pool of potential port numbers for temporary servers
     */
    PeerConn(
            TcpSock&& sock,
            PortPool& portPool);

    /**
     * Client-side construction.
     *
     * @param[in] rmtSrvrAddr         Socket address of the remote server
     * @throws    std::system_error   System error
     * @throws    std::runtime_error  Remote peer closed the connection
     * @cancellationpoint             Yes
     */
    PeerConn(const SockAddr& rmtSrvrAddr);

    /**
     * Returns the socket address of the remote peer. On the client-side, this
     * will be the address of the peer-server; on the server-side, this will be
     * the address of the `accept()`ed socket.
     *
     * @return Socket address of the remote peer.
     */
    const SockAddr& getRmtAddr() const noexcept;

    /**
     * Returns the local socket address.
     *
     * @return Local socket address
     */
    const SockAddr& getLclAddr() const noexcept;

    std::string to_string() const noexcept;

    /**
     * Notifies the remote peer about the availability of a `Chunk`.
     *
     * @param[in] notice  ID of available `Chunk`
     * @threadsafety      Safe
     */
    void notify(const ChunkId& notice);

    /**
     * Requests a `Chunk` from the remote peer.
     *
     * @param[in] request  ID of requested `Chunk`
     * @threadsafety       Safe
     */
    void request(const ChunkId& request);

    /**
     * Sends a `Chunk` to the remote peer.
     *
     * @param[in] chunk  The `Chunk`
     * @threadsafety     Safe
     */
    void send(const MemChunk& chunk);

    /**
     * Receives notification about an available `Chunk` from the remote peer.
     *
     * @return                     ID of the available `Chunk`
     * @throws std::system_error   System error
     * @throws std::runtime_error  Remote peer closed the connection
     * @threadsafety               Safe
     */
    ChunkId getNotice();

    /**
     * Receives a request for a `Chunk` from the remote peer.
     *
     * @return                     ID of the requested `Chunk`
     * @throws std::system_error   System error
     * @throws std::runtime_error  Remote peer closed the connection
     * @threadsafety               Safe
     */
    ChunkId getRequest();

    /**
     * Receives a chunk of data.
     *
     * @return                     Latent chunk of data
     * @throws std::system_error   System error
     * @throws std::runtime_error  Remote peer closed the connection
     */
    TcpChunk getChunk();

    /**
     * Disconnects from the remote peer. After this, nothing can be sent to and
     * nothing will be received from the remote peer. Idempotent.
     */
    void disconnect();
};

} // namespace

#endif /* MAIN_PEER_PEERCONN_H_ */
