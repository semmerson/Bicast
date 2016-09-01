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

#include "PendingPeerConnections.h"

#include <cstring>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>

namespace hycast {

PendingPeerConnections::PendingPeerConnections(unsigned maxPending)
        : map(1, &hash, &areEqual),
          maxPending{maxPending}
{
    if (maxPending == 0)
        throw std::invalid_argument("Maximum number of pending connections is zero");
}

/**
 * Destroys an instance.
 */
PendingPeerConnections::~PendingPeerConnections()
{
    for (auto entry : map) {
        PtrPeerId const pPeerId = entry.first;
        PtrConn const   pConn = entry.second;
        delete pPeerId;
        delete pConn;
    }
    map.clear();
    list.clear();
}

/**
 * Deletes the least-recently-used entry.
 */
void PendingPeerConnections::deleteLru()
{
    PeerId* const pPeerId = list.front();
    map.erase(pPeerId);
    list.pop_front();
    delete pPeerId;
}

/**
 * Returns the entry corresponding to a peer-identifier. Creates the entry if
 * necessary.
 * @param[in] peer_id  Peer identifier
 * @return The corresponding entry
 */
const PendingPeerConnections::Entry* PendingPeerConnections::findOrCreate(
        const PeerId* peer_id)
{
    Entry* entry;
    auto iter = map.find(const_cast<PeerId*>(peer_id));
    if (iter != map.end()) {
        // Existing entry
        entry = &*iter;
        list.remove(entry->first);
        list.push_back(entry->first);
    }
    else {
        // New entry
        PtrPeerId pPeerId = new PeerId(*peer_id);
        PtrConn   pConn = new ServerPeerConnection();
        auto insertion = map.emplace(pPeerId, pConn);
        entry = &*insertion.first;
        list.push_back(pPeerId);
        if (map.size() > maxPending)
            deleteLru();
    }
    return entry;
}

/**
 * Adds a socket. If a `PeerConnectionServer` is returned, then this instance
 * will no longer contain it.
 * @param[in] peer_id   Unique identifier of remote peer
 * @param[in] socket    Socket to be added
 * @return Shared pointer to the completed server-side peer connection or an
 *         empty shared pointer if the connection is incomplete.
 */
std::shared_ptr<ServerPeerConnection> PendingPeerConnections::addSocket(
        const PeerId& peer_id,
        const Socket& socket)
{
    const Entry* entry = findOrCreate(&peer_id);
    PtrConn      pConn = entry->second;
    if (pConn->add_socket(socket)) {
        PtrPeerId pPeerId = entry->first;
        map.erase(pPeerId);
        list.remove(pPeerId);
        delete pPeerId;
        return std::shared_ptr<ServerPeerConnection>(pConn);
    }
    return std::shared_ptr<ServerPeerConnection>();
}

} // namespace
