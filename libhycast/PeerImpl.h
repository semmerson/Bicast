/**
 * This file declares the implementation of a peer. A peer exchanges data with a
 * remote peer over a `PeerConnection`.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Peer.h
 * @author: Steven R. Emmerson
 */

#ifndef PEERIMPL_H_
#define PEERIMPL_H_

#include "PeerConnection.h"

namespace hycast {

class PeerImpl final {
    PeerConnection& conn;
    PeerImpl(const PeerImpl& that);
    PeerImpl& operator=(const PeerImpl& rhs);
public:
    /**
     * Constructs from a `PeerConnection`. Starts executing immediately.
     * @param[in] conn  Connection to remote peer
     * @exceptionsafety Nothrow
     */
    PeerImpl(PeerConnection& conn) noexcept;
};

} // namespace

#endif /* PEERIMPL_H_ */
