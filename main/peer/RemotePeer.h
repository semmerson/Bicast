/**
 * Proxy for a remote peer.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: RemotePeer.h
 *  Created on: May 10, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_PEER_REMOTEPEER_H_
#define MAIN_PEER_REMOTEPEER_H_

#include "Chunk.h"
#include "PeerMsgSndr.h"
#include "PortPool.h"
#include "SockAddr.h"

#include <memory>

namespace hycast {

class RemotePeer
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
    RemotePeer(Impl* const impl);

public:
    /**
     * Server-side construction.
     *
     * @param[in] sock      `::accept()`ed socket
     * @param[in] portPool  Pool of potential port numbers
     */
    RemotePeer(
            Socket&   sock,
            PortPool& portPool);

    /**
     * Client-side construction.
     *
     * @param[in] rmtSrvrAddr            Socket address of the remote server
     * @throws    std::nested_exception  System error
     */
    RemotePeer(const SockAddr& rmtSrvrAddr);

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
     * @return                    ID of the available `Chunk`
     * @throw  std::system_error  Couldn't read chunk ID
     * @threadsafety              Safe
     */
    ChunkId getNotice();

    /**
     * Receives a request for a `Chunk` from the remote peer.
     *
     * @return       ID of the requested `Chunk`
     * @threadsafety Safe
     */
    ChunkId getRequest();

    /**
     * Receives a `Chunk` from the remote peer.
     *
     * @return       The `Chunk`
     * @threadsafety Safe
     */
    WireChunk getChunk();

    /**
     * Disconnects from the remote peer. After this, nothing can be sent to and
     * nothing will be received from the remote peer. Idempotent.
     */
    void disconnect();
};

} // namespace

#endif /* MAIN_PEER_REMOTEPEER_H_ */
