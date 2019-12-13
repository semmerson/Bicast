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
 * A thread-safe queue of chunk identifiers to be sent to a remote peer.
 */
class ChunkIdQueue final
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    ChunkIdQueue();

    size_t size() const noexcept;

    void push(ChunkId chunkId) const;

    ChunkId pop() const;
};

} // namespace

#endif /* MAIN_PEER_THINGIDQUEUE_H_ */
