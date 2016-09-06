/**
 * This file declares a set of pending peer connections on the server-side.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PendingPeerConnection.h
 * @author: Steven R. Emmerson
 */

#ifndef PENDINGPEERCONNECTIONS_H_
#define PENDINGPEERCONNECTIONS_H_

#include "PeerConnection.h"
#include "PeerId.h"
#include "Socket.h"

#include <list>
#include <memory>
#include <unordered_map>
#include <vector>

namespace hycast {

class PendingPeerConnections final {
    typedef std::shared_ptr<PeerId>             PtrPeerId;
    typedef std::vector<Socket>                 SockArray;
    typedef std::shared_ptr<SockArray>          PtrConn;
    typedef std::pair<const PtrPeerId, PtrConn> Entry;

    static size_t hash(const PtrPeerId& pPeerId) {
        return pPeerId->hash();
    }
    static bool areEqual(
            const PtrPeerId& pPeerId1,
            const PtrPeerId& pPeerId2) {
        return pPeerId1->equals(*pPeerId2);
    };

    std::unordered_map<const PtrPeerId, PtrConn,
            decltype(&PendingPeerConnections::hash),
            decltype(&PendingPeerConnections::areEqual)>
                         map;
    std::list<PtrPeerId> list;
    unsigned             maxPending;

    void deleteLru();
    const Entry& findOrCreate(const PeerId& pPeerId);
    /**
     * Adds a socket to a connection.
     * @param[in,out] pConn   The connection
     * @param[in]     socket  The socket to be added
     * @throws std::length_error     if the connection is already complete
     * @throws std::invalid_argument if the connection already has the socket
     * @retval `true`  if the connection is complete
     * @retval `false` if the connection is not complete
     * @exceptionsafety Strong
     */
    static bool add_socket(
            PtrConn&      pConn,
            const Socket& socket);

public:
    /**
     * Constructs from the maximum number of pending connections.
     * @param[in] maxPending  Maximum number of pending connections.
     * @throws std::bad_alloc if required memory can't be allocated
     * @throws std::invalid_argument if `maxPending == 0`
     * @exceptionsafety Strong
     */
    explicit PendingPeerConnections(unsigned maxPending = 32);
    /**
     * Returns the number of pending connections.
     * @exceptionsafety Nothrow
     */
    unsigned numPending() const noexcept {return list.size();}
    /**
     * Adds a socket. If a `PeerConnection` is returned, then this instance will
     * no longer contain it.
     * @param[in] peer_id            Unique identifier of remote peer
     * @param[in] socket             Socket to be added
     * @return                       Shared pointer to the completed server-side
     *                               peer connection or an empty shared pointer
     *                               if the connection is incomplete.
     * @throws std::bad_alloc        if required memory can't be allocated
     * @throws std::invalid_argument if the `PeerConnection` associated with
     *                               `peer_id` already has the socket
     * @exceptionsafety              Strong
     */
    std::shared_ptr<PeerConnection> addSocket(
            const PeerId&       peerId,
            const Socket&       socket);
};

} // namelist

#endif /* PENDINGPEERCONNECTION_H_ */
