/**
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PendingPeerConnection.h
 * @author: Steven R. Emmerson
 *
 * This file declares a set of pending peer connections on the server-side.
 */

#ifndef PENDINGPEERCONNECTIONS_H_
#define PENDINGPEERCONNECTIONS_H_

#include "PeerId.h"
#include "ServerPeerConnection.h"

#include <list>
#include <memory>
#include <unordered_map>

namespace hycast {

class PendingPeerConnections final {
    typedef PeerId*                               PtrPeerId;
    typedef ServerPeerConnection*                 PtrConn;
    typedef std::pair<const PtrPeerId, PtrConn>   Entry;

    static size_t hash(const PtrPeerId pPeerId) {
        return pPeerId->hash();
    }
    static bool areEqual(
            const PtrPeerId pPeerId1,
            const PtrPeerId pPeerId2) {
        return pPeerId1->equals(*pPeerId2);
    };

    std::unordered_map<const PtrPeerId, PtrConn,
            decltype(&PendingPeerConnections::hash),
            decltype(&PendingPeerConnections::areEqual)>
                         map;
    std::list<PtrPeerId> list;
    unsigned             maxPending;

    void deleteLru();
    const Entry* findOrCreate(const PeerId* pPeerId);

public:
    explicit PendingPeerConnections(unsigned maxPending = 32);
    ~PendingPeerConnections();
    unsigned numPending() const {return list.size();}
    std::shared_ptr<ServerPeerConnection> addSocket(
            const PeerId&       peerId,
            const Socket&       socket);
};

} // namelist

#endif /* PENDINGPEERCONNECTION_H_ */
