/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYRIGHT in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PendingPeerConnections.cpp
 * @author: Steven R. Emmerson
 *
 * This file defines a set of pending `PeerConnection`s. A `PeerConnection`
 * isn't complete until it has all three sockets.
 */

#include "PeerConnection.h"
#include "PendingPeerConnections.h"

#include <cstring>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>

namespace hycast {

/**
 * Constructs from the maximum number of pending connections.
 * @param[in] maxPending  Maximum number of pending connections.
 * @throws std::bad_alloc if required memory can't be allocated
 * @throws std::invalid_argument if `maxPending == 0`
 * @exceptionsafety Strong
 */
PendingPeerConnections::PendingPeerConnections(unsigned maxPending)
        : map(1, &hash, &areEqual),
          maxPending{maxPending}
{
    if (maxPending == 0)
        throw std::invalid_argument("Maximum number of pending connections is zero");
}

/**
 * Deletes the least-recently-used entry.
 */
void PendingPeerConnections::deleteLru()
{
    PtrPeerId pPeerId = list.front();
    map.erase(pPeerId);
    list.pop_front();
}

/**
 * Returns the entry corresponding to a peer-identifier. Creates the entry if
 * necessary.
 * @param[in] peer_id  Peer identifier
 * @return The corresponding entry
 * @throws std::bad_alloc if required memory can't be allocated
 * @exceptionsafety Strong
 */
const PendingPeerConnections::Entry& PendingPeerConnections::findOrCreate(
        const PeerId& peer_id)
{
    Entry*    entry;
    PtrPeerId pPeerId{new PeerId(peer_id)};
    auto iter = map.find(pPeerId);
    if (iter != map.end()) {
        // Existing entry
        entry = &*iter;
        list.remove(entry->first);
        list.push_back(entry->first);
    }
    else {
        // New entry
        PtrConn pConn{new SockArray(PeerConnection::max_sockets)};
        pConn->clear();
        auto insertion = map.emplace(pPeerId, pConn);
        entry = &*insertion.first;
        list.push_back(pPeerId);
        if (map.size() > maxPending)
            deleteLru();
    }
    return *entry;
}

bool PendingPeerConnections::add_socket(
            PtrConn&      pConn,
            const Socket& socket)
{
    size_t size = pConn->size();
    if (size == PeerConnection::max_sockets)
        throw std::length_error("Already have " +
                std::to_string(PeerConnection::max_sockets) + " sockets");
    for (size_t i = 0; i < size; i++) {
        if (socket == pConn->at(i))
            throw std::invalid_argument("sockets[" + std::to_string(i) +
                    "] is already socket " + socket.to_string());
    }
    pConn->push_back(socket);
    return pConn->size() == PeerConnection::max_sockets;
}

/**
 * Adds a socket. If a `PeerConnection` is returned, then this instance
 * will no longer contain it.
 * @param[in] peer_id   Unique identifier of remote peer
 * @param[in] socket    Socket to be added
 * @return Shared pointer to the completed server-side peer connection or an
 *         empty shared pointer if the connection is incomplete.
 * @throws std::bad_alloc if required memory can't be allocated
 * @throws std::invalid_argument if the `PeerConnection` associated with
 *                               `peer_id` already has the socket
 * @throws std::length_error     if the connection is already complete
 * @exceptionsafety Strong
 */
std::shared_ptr<PeerConnection> PendingPeerConnections::addSocket(
        const PeerId& peer_id,
        const Socket& socket)
{
    static std::shared_ptr<PeerConnection> incomplete{};
    const Entry& entry = findOrCreate(peer_id);
    PtrConn      pConn = entry.second;
    if (add_socket(pConn, socket)) {
        PtrPeerId pPeerId = entry.first;
        map.erase(pPeerId);
        list.remove(pPeerId);
        return std::shared_ptr<PeerConnection>(
                new PeerConnection(*pConn.get()));
    }
    return incomplete;
}

} // namespace
