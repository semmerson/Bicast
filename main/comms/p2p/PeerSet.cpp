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

#include "ChunkInfo.h"
#include "Completer.h"
#include "error.h"
#include "InetSockAddr.h"
#include "logging.h"
#include "MsgRcvr.h"
#include "Peer.h"
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

    /// An abstract base class for send-actions to be performed on a peer.
    class SendAction
    {
        const TimePoint whenCreated;
    public:
        SendAction()
            : whenCreated{Clock::now()}
        {}
        virtual ~SendAction() =default;
        const TimePoint& getCreateTime() const
        {
            return whenCreated;
        }
        /**
         * Acts upon a peer.
         * @param[in,out] peer  Peer to be acted upon
         * @return `true`       Iff processing should continue
         * @exceptionsafety     No throw
         * @threadsafety        Safe
         */
        virtual void actUpon(Peer& peer) =0;
        virtual bool terminate() const noexcept
        {
            return false;
        }
    };

    /// A send-action notice of a new product.
    class SendProdNotice final : public SendAction
    {
        ProdInfo info;
    public:
        SendProdNotice(const ProdInfo& info)
            : info{info}
        {}
        /**
         * Sends a notice of a data-product to a remote peer.
         * @param[in,out] peer  Peer
         * @exceptionsafety     Basic
         * @threadsafety        Compatible but not safe
         */
        void actUpon(Peer& peer)
        {
            peer.sendNotice(info);
        }
    };

    /// A send-action notice of a new chunk-of-data.
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
         * @param[in,out] peer  Peer
         * @exceptionsafety     Basic
         * @threadsafety        Compatible but not safe
         */
        void actUpon(Peer& peer)
        {
            peer.sendNotice(info);
        }
    };

    /// A send-action that terminates the peer.
    class TerminatePeer final : public SendAction
    {
    public:
        void actUpon(Peer& peer)
        {}
        bool terminate() const noexcept
        {
            return true;
        }
    };

    /**
     * A class that adds behaviors to a peer. Specifically, it creates and
     * manages sending and receiving threads for the peer.
     */
    class PeerWrapper
    {
        /// A queue of send-actions to be performed on a peer.
        class SendQ final
        {
            std::queue<std::shared_ptr<SendAction>> queue;
            std::mutex                              mutex;
            std::condition_variable                 cond;
            bool                                    isDisabled;
        public:
            SendQ()
                : queue{}
                , mutex{}
                , cond{}
                , isDisabled{false}
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
             * @param[in] maxResideTime  Maximum residence time
             * @retval    true           Action was added
             * @retval    false          Action was not added because
             *                           `terminate()` was called
             */
            bool push(
                    std::shared_ptr<SendAction> action,
                    const TimeUnit&             maxResideTime)
            {
                LockGuard lock{mutex};
                if (isDisabled)
                    return false;
                while (!queue.empty() && (Clock::now() -
                        queue.front()->getCreateTime() > maxResideTime))
                    queue.pop();
                queue.push(action);
                cond.notify_one();
                return true;
            }

            std::shared_ptr<PeerSet::Impl::SendAction> pop()
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

            /**
             * Clears the queue and adds a send-action that will terminate
             * the associated peer. Causes `push()` to always fail. Idempotent.
             */
            void terminate()
            {
                UniqueLock lock{mutex};
                while (!queue.empty())
                    queue.pop();
                queue.push(std::shared_ptr<SendAction>(new TerminatePeer()));
                isDisabled = true;
                cond.notify_one();
            }
        };

        typedef int32_t            PeerValue;

        SendQ                      sendQ;
        Peer                       peer;
        std::atomic<PeerValue>     value;
        Completer<void>            completer;

        /**
         * Processes send-actions queued-up for a peer. Doesn't return unless an
         * exception is thrown. Intended to run on its own thread.
         */
        void runSender()
        {
            try {
                for (;;) {
                    std::shared_ptr<SendAction> action{sendQ.pop()};
                    action->actUpon(peer);
                    if (action->terminate())
                        break;
                }
            }
            catch (const std::exception& e) {
                std::throw_with_nested(RUNTIME_ERROR(
                        "Can no longer send to remote peer"));
            }
        }

        /**
         * Causes a peer to receive messages from its associated remote peer.
         * Doesn't return until the remote peer disconnects or an exception is
         * thrown. Intended to run on its own thread.
         */
        void runReceiver()
        {
            try {
                peer.runReceiver();
            }
            catch (const std::exception& e) {
                std::throw_with_nested(RUNTIME_ERROR(
                        "Can no longer receive from remote peer: " +
                        peer.to_string()));
            }
        }

public:
        static const PeerValue VALUE_MAX{INT32_MAX};
        static const PeerValue VALUE_MIN{INT32_MIN};

        /**
         * Constructs from the peer.
         * @param[in] peer           The peer
         * @throw     RuntimeError   Instance couldn't be constructed
         */
        PeerWrapper(Peer& peer)
            : sendQ{}
            , peer{peer}
            , value{0}
            , completer{}
        {}

        /// Prevents copy and move construction and assignment.
        PeerWrapper(const PeerWrapper& that) =delete;
        PeerWrapper(const PeerWrapper&& that) =delete;
        PeerWrapper& operator=(const PeerWrapper& rhs) =delete;
        PeerWrapper& operator=(const PeerWrapper&& rhs) =delete;

        /**
         * Executes the peer. Doesn't return unless an exception is thrown.
         */
        void operator()()
        {
            completer.submit([this]{runReceiver();});
            completer.submit([this]{runSender();});
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

        /**
         * Adds a send-action to the send-action queue.
         * @param[in] action         Send-action to be added
         * @param[in] maxResideTime  Maximum residence time in the queue for
         *                           send-actions
         * @return
         */
        bool push(
                std::shared_ptr<SendAction> action,
                const TimeUnit&              maxResideTime)
        {
            return sendQ.push(action, maxResideTime);
        }
    };

    /// An element in the set of active peers
    typedef std::shared_ptr<PeerWrapper>        PeerEntry;
    /// The future of a peer
    typedef Future<void>                        PeerFuture;

    std::unordered_map<InetSockAddr, PeerEntry> addrToEntryMap;
    std::unordered_map<PeerFuture, PeerEntry>   futureToEntryMap;
    mutable std::mutex                          mutex;
    mutable std::condition_variable             cond;
    const TimeUnit                              stasisDuration;
    const TimeUnit                              maxResideTime;
    std::function<void(InetSockAddr&)>          peerStopped;
    unsigned                                    maxPeers;
    std::exception_ptr                          exception;
    Completer<void>                             completer;
    Thread                                      stoppedPeerThread;
    TimePoint                                   timeLastInsert;

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
        PeerEntry entry{new PeerWrapper(peer)};
        addrToEntryMap.insert({peer.getRemoteAddr(), entry});
        auto peerFuture = completer.submit([entry]() mutable {
                entry->operator()();});
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
            auto peerAddr = iter->second->getPeer().getRemoteAddr();
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
        assert(futureToEntryMap.size() > 0);
        std::pair<PeerFuture, PeerEntry> pair;
        auto minValue = PeerWrapper::VALUE_MAX;
        for (const auto& elt : futureToEntryMap) {
            auto value = elt.second->getValue();
            if (value < minValue) {
                minValue = value;
                pair = elt;
            }
        }
        PeerFuture future = pair.first;
        future.cancel();
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
            elt.second->resetValue();
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
                    mutex.lock();
                    std::pair<bool, InetSockAddr> pair = erasePeer(future);
                    mutex.unlock();
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
     * @param[in] stasisDuration  Minimum amount of time that the set must be
     *                            full and unchanged before the worst-performing
     *                            peer may be removed
     * @param[in] maxPeers        Maximum number of peers
     * @param[in] peerStopped     Function to call when a peer stops
     * @throws InvalidArgument    `maxPeers == 0 || stasisDuration <= 0`
     */
    Impl(   const TimeUnit                     stasisDuration,
            const unsigned                     maxPeers,
            std::function<void(InetSockAddr&)> peerStopped)
        : addrToEntryMap{}
        , futureToEntryMap{}
        , mutex{}
        , cond{}
        , stasisDuration{stasisDuration}
        , maxResideTime{stasisDuration*2}
        , peerStopped{peerStopped}
        , maxPeers{maxPeers}
        , exception{}
        , completer{}
        , stoppedPeerThread{[this]{handleStoppedPeers();}}
        , timeLastInsert{Clock::now()}
    {
        if (maxPeers == 0)
            throw INVALID_ARGUMENT("Maximum number of peers can't be zero");
        if (stasisDuration < TimeUnit{0})
            throw INVALID_ARGUMENT("Stasis duration can't be negative");
    }

    /**
     * Destroys. Cancels all threads and joins them. It is unspecified if peers
     * stopped as a result of this call still report.
     */
    ~Impl()
    {}

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
            if (addrToEntryMap.find(peer.getRemoteAddr()) != addrToEntryMap.end()) {
                inserted = false;
            }
            else {
                if (full())
                    peerAddr = stopAndRemoveWorstPeer();
                add(peer);
                inserted = true;
            }
        }
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
            elt.second->push(action, maxResideTime);
    }

    /**
     * Sends information about a product to the remote peers except for one.
     * @param[in] info            Product information
     * @param[in] except          Address of remote peer to exclude
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
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
            elt.second->push(action, maxResideTime);
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
            elt.second->push(action, maxResideTime);
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
            elt.second->push(action, maxResideTime);
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
                iter->second->incValue();
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
                iter->second->decValue();
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
        const TimeUnit                     stasisDuration,
        const unsigned                     maxPeers,
        std::function<void(InetSockAddr&)> peerStopped)
    : pImpl(new Impl(stasisDuration, maxPeers, peerStopped))
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
