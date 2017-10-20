/**
 * This file implements a set of peers.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerSet.cpp
 * @author: Steven R. Emmerson
 */

#include "Backlogger.h"
#include "ChunkInfo.h"
#include "Completer.h"
#include "error.h"
#include "InetSockAddr.h"
#include "logging.h"
#include "MsgRcvr.h"
#include "Peer.h"
#include "PeerMsgRcvr.h"
#include "PeerSet.h"
#include "ProdInfo.h"
#include "Thread.h"

#include <assert.h>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <cstdint>
#include <functional>
#include <future>
#include <list>
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
 * @return `true`        Iff the mutex is locked
 */
static bool isLocked(std::mutex& mutex)
{
    if (!mutex.try_lock())
        return true;
    mutex.unlock();
    return false;
}

class PeerSet::Impl final
{
    typedef std::mutex                     Mutex;
    typedef std::lock_guard<Mutex>         LockGuard;
    typedef std::unique_lock<Mutex>        UniqueLock;
    typedef std::chrono::time_point<Clock> TimePoint;
    typedef int32_t                        PeerValue;

    static const PeerValue VALUE_MAX{INT32_MAX};
    static const PeerValue VALUE_MIN{INT32_MIN};

    /**
     * Interface for performing sending actions.
     */
    class Sender
    {
    public:
        virtual void sendNotice(const ProdInfo& info) =0;
        virtual void sendNotice(const ChunkInfo& info) =0;
    };

    /// Abstract base class for send-actions.
    class SendAction
    {
        const TimePoint whenCreated;
    public:
        SendAction()
            : whenCreated{Clock::now()}
        {}
        virtual ~SendAction()
        {}
        const TimePoint& getCreateTime() const
        {
            return whenCreated;
        }
        /**
         * Acts upon a `Sender` (i.e., a `PeerEntry`).
         * @param[in,out] sender  Peer entry implementation to be acted upon
         * @return `true`         Processing should continue
         * @return `false`        Processing should stop
         * @exceptionsafety       Basic guarantee
         * @threadsafety          Compatible but not safe
         */
        virtual void actUpon(Sender& sender) =0;
    };

    /// Send-action notice of a new product.
    class SendProdNotice final : public SendAction
    {
        ProdInfo info;
    public:
        SendProdNotice(const ProdInfo& info)
            : info{info}
        {}
        /**
         * Sends a notice of a data-product to a remote peer.
         * @param[in] sender    Peer-entry implementation
         * @exceptionsafety     Basic guarantee
         * @threadsafety        Compatible but not safe
         */
        void actUpon(Sender& sender)
        {
            sender.sendNotice(info);
        }
    };

    /// Send-action notice of a new chunk-of-data.
    class SendChunkNotice final : public SendAction
    {
        ChunkInfo info;
    public:
        SendChunkNotice(const ChunkInfo& info)
            : info{info}
        {}
        /**
         * Sends a notice of the availability of a chunk-of-data to a remote
         * peer.
         * @param[in] sender    Peer-entry implementation
         * @exceptionsafety     Basic guarantee
         * @threadsafety        Compatible but not safe
         */
        void actUpon(Sender& sender)
        {
            sender.sendNotice(info);
        }
    };

    /**
     * An entry in the set of active peers. This class adds attributes to a
     * peer and manages the threads on which the peer executes.
     */
    class PeerEntry final
    {
        class Impl final : public Sender
        {
            /**
             * A queue of send-actions to be performed on an instance.
             */
            class SendQ final
            {
                typedef std::shared_ptr<SendAction> Pimpl;

                std::queue<Pimpl>        queue;
                std::mutex               mutex;
                std::condition_variable  cond;
                const TimeUnit           maxResideTime;
            public:
                SendQ(const TimeUnit& maxResideTime)
                    : queue{}
                    , mutex{}
                    , cond{}
                    , maxResideTime{maxResideTime}
                {}

                /// Prevents copy and move construction and assignment
                SendQ(const SendQ& that) =delete;
                SendQ(const SendQ&& that) =delete;
                SendQ& operator =(const SendQ& q) =delete;
                SendQ& operator =(const SendQ&& q) =delete;

                /**
                 * Adds an action to the queue. Entries older than the maximum
                 * residence time will be deleted.
                 * @param[in] action         Action to be added
                 */
                void push(std::shared_ptr<SendAction> action)
                {
                    LockGuard lock{mutex};
                    while (!queue.empty() && (Clock::now() -
                            queue.front()->getCreateTime() > maxResideTime))
                        queue.pop();
                    queue.push(action);
                    cond.notify_one();
                }

                /**
                 * Removes the entry at the front of the queue and returns it.
                 * @return Entry at front of queue
                 */
                Pimpl pop()
                {
                    UniqueLock lock{mutex};
                    while (queue.empty()) {
                        Canceler canceler{};
                        cond.wait(lock);
                    }
                    auto action = queue.front();
                    queue.pop();
                    return action;
                }
            };

            SendQ                      sendQ;
            Peer                       peer;
            PeerMsgRcvr&               msgRcvr;
            std::atomic<PeerValue>     value;
            Backlogger                 backlogger;
            Completer<void>            completer;

            /**
             * Returns the identity of the chunk of data with which the backlog
             * of notices to the remote peer should start.
             * @param[in] peer    Local peer associated with remote peer
             * @return            Identity of chunk of data
             * @throw LogicError  Next message from remote peer isn't a chunk
             *                    request
             */
            ChunkInfo getStartWithFromRemote(Peer& peer)
            {
                Peer::Message msg = peer.getMessage();
                if (msg.getType() != Peer::MsgType::CHUNK_REQUEST)
                    throw LOGIC_ERROR(
                            "Initial message from remote peer isn't a chunk-request: "
                            "msgType=" + std::to_string(msg.getType()));
                return msg.getChunkInfo();
            }

            /**
             * Processes send-actions queued-up for a peer. Doesn't return
             * unless an exception is thrown. Intended to run on its own thread.
             */
            void runSender()
            {
                try {
                    for (;;) {
                        auto action = sendQ.pop();
                        action->actUpon(*this); // E.g., `sendNotice()`
                    }
                }
                catch (const std::exception& e) {
                    std::throw_with_nested(RUNTIME_ERROR(
                            "Can no longer send to remote peer"));
                }
            }

            /**
             * Receives messages from the remote peer. Doesn't return until the
             * remote peer disconnects or an exception is thrown. Intended to
             * run on its own thread.
             */
            void runReceiver()
            {
                try {
                    for (;;) {
                        auto msg = peer.getMessage();
                        switch (msg.getType()) {
                        case Peer::PROD_NOTICE:
                            msgRcvr.recvNotice(msg.getProdInfo(), peer);
                            break;
                        case Peer::CHUNK_NOTICE:
                            msgRcvr.recvNotice(msg.getChunkInfo(), peer);
                            break;
                        case Peer::PROD_REQUEST:
                            msgRcvr.recvRequest(msg.getProdIndex(), peer);
                            break;
                        case Peer::CHUNK_REQUEST:
                            msgRcvr.recvRequest(msg.getChunkInfo(), peer);
                            break;
                        case Peer::CHUNK:
                            msgRcvr.recvData(msg.getChunk(), peer);
                            break;
                        default: // Peer::EMPTY
                            return;
                        }
                    }
                }
                catch (const std::exception& e) {
                    std::throw_with_nested(RUNTIME_ERROR(
                            "Can't receive from remote peer: " +
                            peer.to_string()));
                }
            }

            /**
             * Notifies the remote peer of the backlog of available data.
             * Intended to run on its own thread.
             */
            void runBacklogger()
            {
                try {
                    backlogger();
                }
                catch (const std::exception& ex) {
                    std::throw_with_nested(RUNTIME_ERROR(
                            "Error notifying remote peer " + peer.to_string() +
                            + " of backlog"));
                }
            }

        public:
            /**
             * Constructs. Sends a (possibly empty) backlog request to the
             * remote peer and receives the remote peer's backlog request.
             * Creates an object to send backlog notices to the remote peer if
             * appropriate.
             * @param[in] peer           Local peer
             * @param[in] msgRcvr        Receiver of message from remote peer
             * @param[in] prodStore      Product storage
             * @param[in] maxResideTime  Maximum residence time for send-actions
             * @throw LogicError         First message from remote peer isn't
             *                           chunk-request
             * @throw RuntimeError       Instance couldn't be constructed
             */
            Impl(   Peer&           peer,
                    PeerMsgRcvr&    msgRcvr,
                    ProdStore&      prodStore,
                    const TimeUnit& maxResideTime)
                : sendQ{maxResideTime}
                , peer{peer}
                , msgRcvr{msgRcvr}
                , value{0}
                , backlogger{}
                , completer{}
            {
                // Configure incoming notices of backlog of available chunks
                auto chunkInfo = prodStore.getOldestMissingChunk();
                peer.sendRequest(chunkInfo);
                chunkInfo = getStartWithFromRemote(peer);
                if (!chunkInfo) {
                    LOG_INFO("Remote peer " + peer.to_string() +
                            " didn't make a backlog request");
                }
                else {
                    backlogger = Backlogger{peer, chunkInfo, prodStore};
                }
            }

            /// Prevents copy and move construction and assignment.
            Impl(const Impl& that) =delete;
            Impl(const Impl&& that) =delete;
            Impl& operator=(const Impl& rhs) =delete;
            Impl& operator=(const Impl&& rhs) =delete;

            InetSockAddr getRemoteAddr()
            {
                return peer.getRemoteAddr();
            }

            /**
             * Executes the peer. Doesn't return unless the remote peer closes
             * the connection or an exception is thrown. Intended to run on its
             * own thread.
             * @exceptionsafety     Basic guarantee
             * @threadsafety        Unsafe
             */
            void operator()()
            {
                completer.submit([this]{runReceiver();});
                completer.submit([this]{runSender();});
                if (backlogger)
                    completer.submit(backlogger);
                completer.take().getResult(); // Will rethrow any task exception
            }

            /**
             * Returns the associated peer.
             * @return The associated peer
             */
            Peer getPeer()
            {
                return peer;
            }

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
             * Decrements the value of the peer.
             * @exceptionsafety Strong
             * @threadsafety    Safe
             */
            void decValue()
            {
                if (value > VALUE_MIN)
                    --value;
            }

            /**
             * Returns the value of the peer.
             * @return The value of the peer
             */
            PeerValue getValue() const
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

            void sendNotice(const ProdInfo& info)
            {
                peer.sendNotice(info);
            }

            void sendNotice(const ChunkInfo& info)
            {
                // No need to include this one in the backlog of available chunks
                backlogger.doNotNotifyOf(info);
                peer.sendNotice(info);
            }

            /**
             * Adds a send-action to the send-action queue.
             * @param[in] action         Send-action to be added
             */
            void push(std::shared_ptr<SendAction> action)
            {
                sendQ.push(action);
            }
        };

        std::shared_ptr<Impl> pImpl;

    public:
        inline PeerEntry()
            : pImpl{}
        {}
        inline PeerEntry(
                Peer&           peer,
                PeerMsgRcvr&    msgRcvr,
                ProdStore&      prodStore,
                const TimeUnit& maxResideTime)
            : pImpl{new Impl(peer, msgRcvr, prodStore, maxResideTime)}
        {}
        InetSockAddr getRemoteAddr() const { return pImpl->getRemoteAddr(); }
        inline void operator()()     const { pImpl->operator()(); }
        inline Peer getPeer()        const { return pImpl->getPeer(); }
        inline void incValue()       const { pImpl->incValue(); }
        inline void decValue()       const { pImpl->decValue(); }
        inline PeerValue getValue()  const { return pImpl->getValue(); }
        inline void resetValue()     const { pImpl->resetValue(); }
        inline void push(
                std::shared_ptr<SendAction> action) const {
            pImpl->push(action);
        }
    };

    /// The future of a peer
    typedef Future<void>                        PeerFuture;

    mutable std::mutex                          mutex;
    mutable std::condition_variable             cond;
    std::unordered_map<InetSockAddr, PeerEntry> addrToEntryMap;
    std::unordered_map<PeerFuture, PeerEntry>   futureToEntryMap;
    ProdStore                                   prodStore;
    const TimeUnit                              stasisDuration;
    const TimeUnit                              maxResideTime;
    std::function<void(InetSockAddr&)>          peerStopped;
    unsigned                                    maxPeers;
    std::exception_ptr                          exception;
    TimePoint                                   timeLastInsert;
    PeerMsgRcvr&                                msgRcvr;
    Completer<void>                             completer;
    Thread                                      stoppedPeerThread;

    /**
     * Indicates if the set is full.
     * @return `true`  Iff set is full
     */
    inline bool full() const
    {
        assert(isLocked(mutex));
        return addrToEntryMap.size() >= maxPeers;
    }

    /**
     * Unconditionally adds a peer. The peer immediately starts receiving
     * messages from its associated remote peer and is ready to send messages.
     * If the inserted peer makes the set of peers full, then all peer values
     * are reset.
     * @param[in] peer   Peer to be inserted
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Compatible but not safe
     */
    void add(Peer& peer)
    {
        assert(isLocked(mutex));
        PeerEntry entry{peer, msgRcvr, prodStore, maxResideTime};
        PeerFuture peerFuture{};
        {
            UnlockGuard unlock{mutex};
            peerFuture = completer.submit([entry]() {entry.operator()();});
        }
        addrToEntryMap.insert({peer.getRemoteAddr(), entry});
        futureToEntryMap.insert({peerFuture, entry});
        timeLastInsert = Clock::now();
        if (full())
            resetValues();
    }

    std::pair<bool, InetSockAddr> erasePeer(PeerFuture& future)
    {
        assert(isLocked(mutex));
        auto iter = futureToEntryMap.find(future);
        if (iter != futureToEntryMap.end()) {
            auto peerAddr = iter->second.getRemoteAddr();
            addrToEntryMap.erase(peerAddr);
            futureToEntryMap.erase(iter);
            return {true, peerAddr};
        }
        return {false, InetSockAddr()};
    }

    /**
     * Stops the peer with the lowest value in the set of active peers and
     * removes it from the set.
     * @exceptionsafety  Basic guarantee
     * @threadsafety     Compatible but not safe
     * @return           Address of remote peer that was removed
     */
    InetSockAddr stopAndRemoveWorstPeer()
    {
        assert(isLocked(mutex));
        std::pair<PeerFuture, PeerEntry> pair;
        auto minValue = VALUE_MAX;
        for (const auto& elt : futureToEntryMap) {
            auto value = elt.second.getValue();
            if (value < minValue) {
                minValue = value;
                pair = elt;
            }
        }
        auto future = pair.first;
        if (future) {
            UnlockGuard unlock{mutex};
            future.cancel();
        }
        return erasePeer(future).second;
    }

    /**
     * Resets the value-counts in the peer-to-value map.
     * @exceptionsafety  Nothrow
     * @threadsafety     Compatible but not safe
     */
    void resetValues() noexcept
    {
        assert(isLocked(mutex));
        for (const auto& elt : addrToEntryMap)
            elt.second.resetValue();
    }

    /**
     * Handles stopped peers in the set of active peers. Removes a stopped peer
     * from the set and notifies the stopped-peer observer. Doesn't return
     * unless an exception is thrown. Intended to run on its own thread.
     */
    void handleStoppedPeers()
    {
    	try {
    	    try {
                for (;;) {
                    auto future = completer.take(); // Blocks
                    try {
                        future.getResult();
                    }
                    catch (const std::exception& ex) {
                        log_warn(ex);
                    }
                    std::pair<bool, InetSockAddr> pair;
                    {
                        LockGuard lock{mutex};
                        pair = erasePeer(future);
                    }
                    if (pair.first)
                        peerStopped(pair.second);
                }
            }
    	    catch (const std::exception& e) {
    	        std::throw_with_nested(RUNTIME_ERROR(
    	                "Error handling stopped peers"));
    	    }
    	}
    	catch (const std::exception& e) {
    	    LockGuard lock{mutex};
            exception = std::current_exception();
    	}
    }

public:
    /**
     * Constructs from the maximum number of peers. The set will be empty.
     * @param[in] prodStore       Product storage
     * @param[in] msgRcvr         Receiver of messages from remote peer
     * @param[in] stasisDuration  Minimum amount of time that the set must be
     *                            full and unchanged before the worst-performing
     *                            peer may be removed
     * @param[in] maxPeers        Maximum number of peers
     * @param[in] peerStopped     Function to call when a peer stops
     * @throws InvalidArgument    `maxPeers == 0 || stasisDuration <= 0`
     */
    Impl(   ProdStore&                         prodStore,
            PeerMsgRcvr&                       msgRcvr,
            const TimeUnit                     stasisDuration,
            const unsigned                     maxPeers,
            std::function<void(InetSockAddr&)> peerStopped)
        : mutex{}
        , cond{}
        , addrToEntryMap{}
        , futureToEntryMap{}
        , prodStore{prodStore}
        , stasisDuration{stasisDuration}
        , maxResideTime{stasisDuration*2}
        , peerStopped{peerStopped}
        , maxPeers{maxPeers}
        , exception{}
        , timeLastInsert{Clock::now()}
        , msgRcvr{msgRcvr}
        , completer{}
        , stoppedPeerThread{[this]{handleStoppedPeers();}}
    {
        if (maxPeers == 0)
            throw INVALID_ARGUMENT("Maximum number of peers can't be zero");
        if (stasisDuration < TimeUnit{0})
            throw INVALID_ARGUMENT("Stasis duration can't be negative");
        // Check whether a backlog of data-chunks will be requested
        auto chunkInfo = prodStore.getOldestMissingChunk();
        if (!chunkInfo)
            LOG_NOTE("No oldest missing data-chunk => no backlog will "
                    "be requested");
    }

    /**
     * Destroys. Cancels all threads and joins them. It is unspecified if peers
     * stopped as a result of this call still report.
     */
    ~Impl() =default;

    /// Prevents copy and move construction and assignment
    Impl(const Impl& that) =delete;
    Impl(const Impl&& that) =delete;
    Impl& operator =(const Impl& rhs) =delete;
    Impl& operator =(const Impl&& rhs) =delete;

    /**
     * Tries to insert a peer. The attempt will fail if the peer is already a
     * member. If the set is full, then
     *   - The current thread is blocked until the membership has been unchanged
     *     for at least the amount of time given to the constructor; and
     *   - Upon return, the worst-performing will have been removed from the
     *     set.
     * @param[in]  peer     Candidate peer
     * @return     `false`  Peer is already a member
     * @return     `true`   Peer was added. Worst peer removed and reported if
     *                      the set was full.
     * @exceptionsafety     Strong guarantee
     * @threadsafety        Safe
     */
    bool tryInsert(Peer& peer)
    {
        bool         inserted;
        InetSockAddr peerAddr{};
        {
            UniqueLock lock{mutex};
            if (exception)
                std::rethrow_exception(exception);
            for (auto timeNextAttempt = timeLastInsert + stasisDuration;
                    full() && Clock::now() < timeNextAttempt;
                    timeNextAttempt = timeLastInsert + stasisDuration) {
                Canceler canceler{};
                cond.wait_until(lock, timeNextAttempt);
            }
            if (addrToEntryMap.find(peer.getRemoteAddr()) !=
                    addrToEntryMap.end()) {
                inserted = false;
            }
            else {
                if (full())
                    peerAddr = stopAndRemoveWorstPeer();
                add(peer);
                inserted = true;
            }
        } // `mutex` locked
        if (peerAddr)
            peerStopped(peerAddr);
        return inserted;
    }

    /**
     * Indicates if this instance already has a given remote peer.
     * @param[in] peerAddr  Address of remote peer
     * @retval `true`       The peer is a member
     * @retval `false`      The peer is not a member
     */
    bool contains(const InetSockAddr& peerAddr) const
    {
        LockGuard lock{mutex};
    	if (exception)
            std::rethrow_exception(exception);
        return addrToEntryMap.find(peerAddr) != addrToEntryMap.end();
    }

    /**
     * Sends information about a product to the remote peers.
     * @param[in] info            Product information
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Safe
     */
    void sendNotice(const ProdInfo& info)
    {
        LockGuard lock{mutex};
    	if (exception)
            std::rethrow_exception(exception);
        std::shared_ptr<SendProdNotice> action{new SendProdNotice(info)};
        for (const auto& elt : addrToEntryMap)
            elt.second.push(action);
    }

    /**
     * Sends information about a product to the remote peers except for one.
     * @param[in] info            Product information
     * @param[in] except          Address of remote peer to exclude
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic guarantee
     * @threadsafety              Safe
     */
    void sendNotice(
            const ProdInfo&     info,
            const InetSockAddr& except)
    {
        LockGuard lock{mutex};
    	if (exception)
            std::rethrow_exception(exception);
        std::shared_ptr<SendProdNotice> action{new SendProdNotice(info)};
        for (const auto& elt : addrToEntryMap) {
            if (elt.first == except)
                continue;
            elt.second.push(action);
        }
    }

    /**
     * Sends information about a chunk-of-data to the remote peers.
     * @param[in] info            Chunk information
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Safe
     */
    void sendNotice(const ChunkInfo& info)
    {
        LockGuard lock{mutex};
    	if (exception)
            std::rethrow_exception(exception);
        std::shared_ptr<SendChunkNotice> action{new SendChunkNotice(info)};
        for (const auto& elt : addrToEntryMap)
            elt.second.push(action);
    }

    /**
     * Sends information about a chunk-of-data to the remote peers except for
     * one.
     * @param[in] info            Chunk information
     * @param[in] except          Address of remote peer to exclude
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Safe
     */
    void sendNotice(const ChunkInfo& info, const InetSockAddr& except)
    {
        LockGuard lock{mutex};
    	if (exception)
            std::rethrow_exception(exception);
        std::shared_ptr<SendChunkNotice> action{new SendChunkNotice(info)};
        for (const auto& elt : addrToEntryMap) {
            if (elt.first == except)
                continue;
            elt.second.push(action);
        }
    }

    /**
     * Increments the value of a peer. Does nothing if the peer isn't found.
     * @param[in] peer  Peer to have its value incremented
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    void incValue(Peer& peer)
    {
        LockGuard lock{mutex};
    	if (exception)
            std::rethrow_exception(exception);
        if (full()) {
            auto iter = addrToEntryMap.find(peer.getRemoteAddr());
            if (iter != addrToEntryMap.end())
                iter->second.incValue();
        }
    }

    /**
     * Decrements the value of a peer. Does nothing if the peer isn't found.
     * @param[in] peer  Peer to have its value decremented
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    void decValue(Peer& peer)
    {
        LockGuard lock{mutex};
    	if (exception)
            std::rethrow_exception(exception);
        if (full()) {
            auto iter = addrToEntryMap.find(peer.getRemoteAddr());
            if (iter != addrToEntryMap.end())
                iter->second.decValue();
        }
    }

    /**
     * Returns the number of peers in the set.
     * @return Number of peers in the set
     */
    size_t size() const
    {
        LockGuard lock{mutex};
    	if (exception)
            std::rethrow_exception(exception);
        return addrToEntryMap.size();
    }

    /**
     * Indicates if this instance is full.
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    bool isFull() const
    {
        return size() >= maxPeers;
    }
};

PeerSet::PeerSet(
        ProdStore&                         prodStore,
        PeerMsgRcvr&                       msgRcvr,
        const TimeUnit                     stasisDuration,
        const unsigned                     maxPeers,
        std::function<void(InetSockAddr&)> peerStopped)
    : pImpl(new Impl(prodStore, msgRcvr, stasisDuration, maxPeers, peerStopped))
{}

bool PeerSet::tryInsert(Peer& peer) const
{
    return pImpl->tryInsert(peer);
}

bool PeerSet::contains(const InetSockAddr& peerAddr) const
{
    return pImpl->contains(peerAddr);
}

void PeerSet::sendNotice(const ProdInfo& prodInfo) const
{
    pImpl->sendNotice(prodInfo);
}

void PeerSet::sendNotice(const ProdInfo& prodInfo, const InetSockAddr& except) const
{
    pImpl->sendNotice(prodInfo, except);
}

void PeerSet::sendNotice(const ChunkInfo& chunkInfo) const
{
    pImpl->sendNotice(chunkInfo);
}

void PeerSet::sendNotice(const ChunkInfo& chunkInfo, const InetSockAddr& except) const
{
    pImpl->sendNotice(chunkInfo, except);
}

void PeerSet::incValue(Peer& peer) const
{
    pImpl->incValue(peer);
}

void PeerSet::decValue(Peer& peer) const
{
    pImpl->decValue(peer);
}

size_t PeerSet::size() const
{
    return pImpl->size();
}

bool PeerSet::isFull() const
{
    return pImpl->isFull();
}

} // namespace
