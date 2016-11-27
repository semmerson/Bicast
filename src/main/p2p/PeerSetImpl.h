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

#include <condition_variable>
#include <chrono>
#include <cstdint>
#include <future>
#include <map>
#include <mutex>
#include <queue>
#include <set>
#include <thread>
#include <unordered_map>
#include <utility>

namespace hycast {

/**
 * Indicates if a recursive mutex is locked by the current thread.
 * @param[in,out] mutex  The mutex
 * @return `true` iff the mutex is locked
 */
bool isLocked(std::recursive_mutex& mutex);

class PeerSetImpl final {
    typedef std::chrono::seconds                               TimeRes;
    typedef std::chrono::time_point<std::chrono::steady_clock> Time;
    typedef std::chrono::steady_clock                          Clock;

    class PeerAction {
        Time whenCreated;
    public:
        PeerAction()
            : whenCreated{Clock::now()} {}
        virtual ~PeerAction() =default;
        Time getCreateTime() const {
            return whenCreated;
        }
        /**
         * Acts upon a peer.
         * @param[in,out] peer  Peer to be acted upon
         * @return `true` iff processing should continue
         */
        virtual void actUpon(const Peer& peer) const =0;
        virtual bool terminate() const { return false; }
    };

    class SendProdNotice final : public PeerAction {
        ProdInfo info;
    public:
        SendProdNotice(const ProdInfo& info)
            : info{info} {}
        /**
         * Sends a notice of a data-product to a remote peer.
         * @param[in,out] peer  Peer
         * @exceptionsafety  Basic
         * @threadsafety     Compatible but not safe
         */
        void actUpon(const Peer& peer) const { peer.sendNotice(info); }
    };

    class SendChunkNotice final : public PeerAction {
        ChunkInfo info;
    public:
        SendChunkNotice(const ChunkInfo& info)
            : info{info} {}
        /**
         * Sends a notice of the availability of a chunk-of-data to a remote
         * peer.
         * @param[in,out] peer  Peer
         * @exceptionsafety  Basic
         * @threadsafety     Compatible but not safe
         */
        void actUpon(const Peer& peer) const { peer.sendNotice(info); }
    };

    class TerminatePeer final : public PeerAction {
    public:
        void actUpon(const Peer& peer) const {}
        bool terminate() const { return true; }
    };

    class PeerActionQ final {
        std::queue<std::shared_ptr<PeerAction>>  queue;
        std::mutex                               mutex;
        std::condition_variable                  cond;

    public:
        PeerActionQ()
            : queue{},
              mutex{},
              cond{} {}
        PeerActionQ(const PeerActionQ& that)
            : queue{that.queue} {}
        bool push(
                std::shared_ptr<PeerAction> action,
                const TimeRes&              maxResideTime);
        void sendNotice(
                const ProdInfo& prodInfo,
                const TimeRes&  maxResideTime);
        void sendNotice(
                const ChunkInfo& chunkInfo,
                const TimeRes&   maxResideTime);
        PeerActionQ& operator =(const PeerActionQ& q) =delete;
        std::shared_ptr<PeerAction> pop();
        /**
         * Clears the queue and adds an action that will terminate the
         * associated peer.
         */
        void terminate();
    };

    class PeerMap {
        struct PeerEntryImpl {
            PeerActionQ                  actionQ;
            static const uint32_t        VALUE_MAX{UINT32_MAX};
            mutable std::recursive_mutex mutex;
            uint32_t                     value;
            const TimeRes                maxResideTime;
        };
        typedef std::shared_ptr<PeerEntryImpl> PeerEntry;
        std::unordered_map<Peer, PeerEntry>    map;
        unsigned                               maxPeers;
    };

    unsigned                            maxPeers;
    std::map<Peer, PeerActionQ>         actionQs;
    static const uint32_t               VALUE_MAX{UINT32_MAX};
    std::unordered_map<Peer, uint32_t>  values;
    // Recursive because terminate() called while locked and unlocked
    mutable std::recursive_mutex        mutex;
    bool                                incValueEnabled;
    Time                                whenEligible;
    const TimeRes                       eligibilityDuration;
    const TimeRes                       maxResideTime;

    /**
     * Removes a peer from the internal data structures.
     * @param[in] peer  Peer to be removed
     */
    void remove(const Peer& peer);
    /**
     * Terminates a peer in the set of peers and removes it from the set.
     * @param[in,out] peer  Peer to be terminated and removed
     */
    void terminate(const Peer& peer);
    /**
     * Processes actions queued-up for a peer.
     * @param[in,out] peer     The peer
     * @param[in,out] actionQ  The action queue
     */
    void processPeerActions(
            const Peer&  peer,
            PeerActionQ& actionQ);
    /**
     * Causes a peer to receive messages from its associated remote peer.
     * Doesn't return until the remote peer disconnects or an exception is
     * thrown, Calls remove() before returning.
     * @param[in,out] peer  Peer to receive messages
     */
    void runReceiver(const Peer& peer);
    /**
     * Unconditionally inserts a peer. The peer immediately starts receiving
     * messages from its associated remote peer and is ready to send messages.
     * @param[in] peer  Peer to be inserted
     * @exceptionsafety Strong guarantee
     * @threadsafety    Compatible but not safe
     */
    void insert(const Peer& peer);
    /**
     * Sets the time-point when the worst performing peer may be removed from
     * the set of peers.
     */
    void setWhenEligible();
    /**
     * Unconditionally removes the worst performing peer from the set of peers.
     * @return The removed peer
     * @exceptionsafety Nothrow
     * @threadsafety    Compatible but not safe
     */
    Peer removeWorstPeer();
    /**
     * Resets the value-counts in the map from peer to value.
     * @exceptionsafety Nothrow
     * @threadsafety    Compatible but not safe
     */
    void resetValues();

public:
    typedef enum {
        FAILURE,
        SUCCESS,
        REPLACED
    } InsertStatus;
    /**
     * Constructs from the maximum number of peers. The set will be empty.
     * @param[in] maxPeers     Maximum number of peers
     * @param[in] minDuration  Required duration before the worst-performing
     *                         peer may be replaced
     * @throws std::invalid_argument if `maxPeers == 0`
     */
    PeerSetImpl(
            unsigned maxPeers,
            TimeRes  minDuration);
    /**
     * Tries to insert a peer.
     * @param[in]  candidate Candidate peer
     * @param[out] worst     Replaced, worst-performing peer
     * @return The status of the insertion. `*worst` is set if the returned
     *         status is `REPLACED`.
     * @exceptionsafety Strong guarantee
     * @threadsafety    Safe
     */
    InsertStatus tryInsert(
            Peer& candidate,
            Peer* worst);
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
};

} // namespace

#endif /* PEERSETIMPL_H_ */
