/**
 * This file declares the implementation of a set of peers.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerSetImpl.h
 * @author: Steven R. Emmerson
 */

#ifndef PEERSETIMPL_H_
#define PEERSETIMPL_H_

#include "ChunkInfo.h"
#include "Peer.h"
#include "ProdInfo.h"

#include <cstdint>
#include <mutex>
#include <set>
#include <unordered_map>
#include <utility>

namespace hycast {

class PeerSetImpl final {
    struct ValueEntry {
        // @see removeWorstEligible() for the reason for the "-1"
        static const uint32_t VALUE_MAX{UINT32_MAX-1};
        uint32_t              value;
        bool                  isEligible;
        ValueEntry()
            : value{0},
              isEligible{false} {} // Just-added peer isn't eligible for removal
    };

    unsigned                             maxPeers;
    std::set<Peer>                       set;
    std::unordered_map<Peer, ValueEntry> values;
    std::mutex                           mutex;

    /**
     * Removes the worst performing peer that's eligible for removal.
     * @return a pair whose first member indicates if the second member exists
     * @exceptionsafety Nothrow
     * @threadsafety    Compatible but not safe
     */
    std::pair<bool, Peer> removeWorstEligible();
    /**
     * Resets the entries in the map from peer to value. Makes all peers
     * eligible for removal.
     * @exceptionsafety Nothrow
     * @threadsafety    Compatible but not safe
     */
    void resetValues();

public:
    /**
     * Constructs from the maximum number of peers. The set will be empty.
     * @throws std::invalid_argument if `maxPeers == 0`
     */
    PeerSetImpl(unsigned maxPeers);
    /**
     * Inserts a peer.
     * @param[in] peer  Peer to be inserted
     * @return `true` iff the peer was inserted
     * @exceptionsafety Strong guarantee
     * @threadsafety    Safe
     */
    bool insert(Peer& peer);
    /**
     * Sends information about a product to the remote peers.
     * @param[in] prodInfo  Product information
     * @throws std::system_error if an I/O error occurs
     * @exceptionsafety Basic
     * @threadsafety    Safe
     */
    void sendNotice(const ProdInfo& prodInfo);
    /**
     * Sends information about a chunk-of-data to the remote peers.
     * @param[in] chunkInfo  Chunk information
     * @throws std::system_error if an I/O error occurs
     * @exceptionsafety Basic
     * @threadsafety    Safe
     */
    void sendNotice(const ChunkInfo& chunkInfo);
    /**
     * Increments the value of a peer. Does nothing if the peer isn't found.
     * @param[in] peer  Peer to have its value incremented
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    void incValue(const Peer& peer);
    /**
     * Possibly removes the worst performing peer from the set and returns it.
     * Does nothing if the set isn't full.
     * @return a pair whose first member indicates if the second member exists
     * @exceptionsafety Basic
     * @threadsafety    Safe
     */
    std::pair<bool, Peer> possiblyRemoveWorst();
};

} // namespace

#endif /* PEERSETIMPL_H_ */
