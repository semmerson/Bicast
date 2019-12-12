/**
 * A thread-safe queue of things to be sent to remote peers.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: ThingIdQueue.h
 *  Created on: Jun 18, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_PEER_THINGIDQUEUE_H_
#define MAIN_PEER_THINGIDQUEUE_H_

#include "error.h"
#include "hycast.h"
#include "PeerProto.h"

#include <memory>

namespace hycast {

/**
 * An identifier of a thing. The identifier will be sent to a remote peer.
 */
class ThingId final
{
    /*
     * Implemented as a discriminated union in order to have a fixed size in
     * a container.
     */
    bool isProd;

    union {
        ProdIndex prodIndex;
        SegId     segId;
    } id;

public:
    ThingId(ProdIndex prodIndex)
        : isProd{true}
        , id{.prodIndex=prodIndex}
    {}

    ThingId(const SegId& segId)
        : isProd{false}
        , id{.segId=segId}
    {}

    ThingId()
        : ThingId(SegId{})
    {}

    void notify(PeerProto& peerProto) const;

    void request(PeerProto& peerProto) const;
};

/******************************************************************************/

/**
 * A thread-safe queue of identifiers to be sent to a remote peer.
 */
class IdQueue final
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    IdQueue();

    size_t size() const noexcept;

    void push(ProdIndex prodIndex) const;

    void push(const SegId& segId) const;

    ThingId pop() const;
};

} // namespace

#endif /* MAIN_PEER_THINGIDQUEUE_H_ */
