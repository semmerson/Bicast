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
#include <functional>
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
 * Indicates if a mutex is locked by the current thread.
 * @param[in,out] mutex  The mutex
 * @return `true` iff the mutex is locked
 */
bool isLocked(std::mutex& mutex);

class PeerSetImpl final {
    typedef std::chrono::seconds                               TimeRes;
    typedef std::chrono::time_point<std::chrono::steady_clock> Time;
    typedef std::chrono::steady_clock                          Clock;

    /**
     * An abstract base class for send-actions to be performed on a peer.
     */
    class SendAction {
        Time whenCreated;
    public:
        SendAction()
            : whenCreated{Clock::now()} {}
        virtual ~SendAction() =default;
        Time getCreateTime() const {
            return whenCreated;
        }
        /**
         * Acts upon a peer.
         * @param[in,out] peer  Peer to be acted upon
         * @return `true` iff processing should continue
         */
        virtual void actUpon(Peer& peer) const =0;
        virtual bool terminate() const { return false; }
    };

    /**
     * A send-action notice of a new product.
     */
    class SendProdNotice final : public SendAction {
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
        void actUpon(Peer& peer) const { peer.sendNotice(info); }
    };

    /**
     * A send-action notice of a new chunk-of-data.
     */
    class SendChunkNotice final : public SendAction {
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
        void actUpon(Peer& peer) const { peer.sendNotice(info); }
    };

    /**
     * A send-action that terminates the peer.
     */
    class TerminatePeer final : public SendAction {
    public:
        void actUpon(Peer& peer) const {}
        bool terminate() const { return true; }
    };

    /**
     * A queue of send-actions to be performed on a peer.
     */
    class SendQ final {
        std::queue<std::shared_ptr<SendAction>>  queue;
        std::mutex                               mutex;
        std::condition_variable                  cond;
    public:
        SendQ()
            : queue{},
              mutex{},
              cond{} {}
        SendQ(const SendQ& that)
            : queue{that.queue}
        {}
        bool push(
                std::shared_ptr<SendAction> action,
                const TimeRes&                maxResideTime);
        SendQ& operator =(const SendQ& q) =delete;
        std::shared_ptr<SendAction> pop();
        /**
         * Clears the queue and adds an send-action that will terminate the
         * associated peer.
         */
        void terminate();
    };

    /**
     * The entry for an active peer.
     */
    class PeerEntryImpl {
        SendQ                      sendQ;
        Peer                       peer;
        std::function<void(Peer&)> handleFailure;
        uint32_t                   value;
        std::thread                sendThread;
        std::thread                recvThread;
    public:
        static const uint32_t VALUE_MAX{UINT32_MAX};
        /**
         * Constructs from the peer. Immediately starts receiving and sending.
         * @param[in] peer  The peer
         */
        PeerEntryImpl(Peer& peer, std::function<void(Peer&)> handleFailure)
            : sendQ{}
            , peer{peer}
            , handleFailure{handleFailure}
            , value{0}
            , sendThread{std::thread([=]{ processSendQ(); })}
            , recvThread{std::thread([=]{ runReceiver(); })}
        {}
        /**
         * Destroys.
         */
        ~PeerEntryImpl();
        /**
         * Prevents copy assignment and move assignment.
         */
        PeerEntryImpl& operator=(PeerEntryImpl& rhs) =delete;
        PeerEntryImpl& operator=(PeerEntryImpl&& rhs) =delete;
        /**
         * Processes send-actions queued-up for a peer. Doesn't return until
         * the sentinel send-action is seen or an exception is thrown.
         */
        void processSendQ();
        /**
         * Causes a peer to receive messages from its associated remote peer.
         * Doesn't return until the remote peer disconnects or an exception is
         * thrown.
         */
        void runReceiver();
        /**
         * Increments the value of the peer.
         * @exceptionsafety Strong
         * @threadsafety    Safe
         */
        void incValue()
        {
            if (value < VALUE_MAX)
                ++value;
        }
        /**
         * Returns the value of the peer.
         * @return The value of the peer
         */
        uint32_t getValue() const
        {
            return value;
        }
        /**
         * Resets the value of a peer.
         * @exceptionsafety Nothrow
         * @threadsafety    Compatible but not safe
         */
        void resetValue()
        {
            value = 0;
        }
        /**
         * Adds a send-action to the send-action queue.
         * @param[in] action         Send-action to be added
         * @param[in] maxResideTime  Maximum residence time in the queue for
         *                           send-actions
         * @return
         */
        bool push(
                std::shared_ptr<SendAction> action,
                const TimeRes&              maxResideTime)
        {
            return sendQ.push(action, maxResideTime);
        }
    };

    typedef std::shared_ptr<PeerEntryImpl> PeerEntry;

    std::unordered_map<Peer, PeerEntry> peerEntries;
    mutable std::mutex                  mutex;
    Time                                whenEligible;
    const TimeRes                       eligibilityDuration;
    const TimeRes                       maxResideTime;
    unsigned                            maxPeers;
    bool                                incValueEnabled;

    /**
     * Unconditionally inserts a peer. The peer immediately starts receiving
     * messages from its associated remote peer and is ready to send messages.
     * @param[in] peer  Peer to be inserted
     * @exceptionsafety Strong guarantee
     * @threadsafety    Compatible but not safe
     */
    void insert(Peer& peer);
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
    /**
     * Handles failure of a peer.
     * @param[in] peer  The peer that failed
     */
    void handleFailure(Peer& peer);

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
    void incValue(Peer& peer);
};

} // namespace

#endif /* PEERSETIMPL_H_ */
