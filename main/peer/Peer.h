/**
 * A local peer that communicates with its associated remote peer. Besides
 * sending notices to the remote peer, this class also creates and runs
 * independent threads that receive messages from a remote peer and pass them to
 * a peer message receiver.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Peer.h
 *  Created on: May 10, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_PEER_PEER_H_
#define MAIN_PEER_PEER_H_

#include "Chunk.h"
#include "PeerConn.h"
#include "PeerMsgRcvr.h"
#include "PeerMsgSndr.h"

#include <memory>
#include <unordered_set>

namespace hycast {

class Peer
{
protected:
    class Impl;

    std::shared_ptr<Impl> pImpl;

public:
    typedef std::unordered_set<ChunkId>::const_iterator iterator;

    /**
     * Default construction.
     */
    Peer();

    /**
     * Constructs from a connection to a remote peer and a receiver of messages
     * from the remote peer.
     *
     * @param[in] peerConn  Connection to the remote peer
     * @param[in] msgRcvr   The receiver of messages from the remote peer
     */
    Peer(   PeerConn   peerConn,
            PeerMsgRcvr& msgRcvr);

    /**
     * Copy construction.
     *
     * @param[in] peer  Peer to be copied
     */
    Peer(const Peer& peer);

    ~Peer() noexcept;

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

    operator bool() const noexcept;

    Peer& operator=(const Peer& rhs);

    bool operator==(const Peer& rhs) const noexcept;

    bool operator<(const Peer& rhs) const noexcept;

    /**
     * Executes asynchronous tasks that call the member functions of the
     * constructor's `PeerMsgRcvr` argument. Doesn't return until the current
     * thread is canceled or a task throws an exception.
     *
     * @throws    std::system_error   System error
     * @throws    std::runtime_error  Remote peer closed the connection
     */
    void operator ()();

    /**
     * Halts execution. Terminates all subtasks. Causes `operator()()` to
     * return. If `terminate()` is called before this method, then this instance
     * will return immediately and won't execute. Idempotent.
     *
     * @cancellationpoint No
     */
    void halt() noexcept;

    /**
     * Notifies the remote peer about the availability of a `Chunk` by enqueuing
     * a notice.
     *
     * @param[in] notice   ID of available `Chunk`
     * @retval    `true`   Notice was enqueued to be sent
     * @retval    `true`   Notice was not enqueued to be sent
     */
    bool notify(const ChunkId& notice) const;

    size_t size() const noexcept;

    iterator begin() const noexcept;

    iterator end() const noexcept;

    size_t hash() const noexcept;

    std::string to_string() const noexcept;
};

} // namespace

namespace std {
    template<>
    struct hash<hycast::Peer>
    {
        size_t operator()(const hycast::Peer& peer) const noexcept
        {
            return peer.hash();
        }
    };

    template<>
    struct equal_to<hycast::Peer>
    {
        size_t operator()(
                const hycast::Peer& peer1,
                const hycast::Peer& peer2) const noexcept
        {
            return peer1 == peer2;
        }
    };
}

#endif /* MAIN_PEER_PEER_H_ */
