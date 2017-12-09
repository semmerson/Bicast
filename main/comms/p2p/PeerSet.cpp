/**
 * This file implements a set of active peers.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerSet.cpp
 * @author: Steven R. Emmerson
 */

#include "Completer.h"
#include "error.h"
#include "logging.h"
#include "PeerSet.h"
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
#include <unordered_map>
#include <utility>

namespace hycast {

/**
 * Indicates if a non-recursive mutex is locked by the current thread.
 * @param[in,out] mutex  Non-recursive mutex
 * @return `true`        Mutex is locked
 */
static bool isLocked(std::mutex& mutex)
{
    if (!mutex.try_lock())
        return true;
    mutex.unlock();
    return false;
}

/******************************************************************************/

/**
 * API presented to a `PeerEntry::Impl` by a `PeerSet::Impl`.
 */
class PeerEntryServer : public P2pServer
                      , public BackloggerFactory
                      , public P2pContentRcvr
{
public:
    virtual ~PeerEntryServer() =default;
};

/******************************************************************************/

class PeerSet::Impl final : public PeerEntryServer
{
    typedef std::mutex                     Mutex;
    typedef std::lock_guard<Mutex>         LockGuard;
    typedef std::unique_lock<Mutex>        UniqueLock;
    typedef std::chrono::steady_clock      Clock;
    typedef std::chrono::time_point<Clock> TimePoint;
    typedef std::chrono::seconds           TimeUnit;
    typedef int32_t                        PeerValue;

    static const PeerValue VALUE_MAX{INT32_MAX};
    static const PeerValue VALUE_MIN{INT32_MIN};

    /**
     * Interface for performing sending actions.
     */
    class Sender
    {
    public:
        virtual void notify(const ProdIndex& prodIndex) =0;
        virtual void notify(const ChunkId& id) =0;
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
        ProdIndex prodIndex;
    public:
        SendProdNotice(const ProdIndex& prodIndex)
            : prodIndex{prodIndex}
        {}
        /**
         * Sends a notice of a data-product to a remote peer.
         * @param[in] sender    Peer-entry implementation
         * @exceptionsafety     Basic guarantee
         * @threadsafety        Compatible but not safe
         */
        void actUpon(Sender& sender)
        {
            sender.notify(prodIndex);
        }
    };

    /// Send-action notice of a new chunk-of-data.
    class SendChunkNotice final : public SendAction
    {
        ChunkId id;
    public:
        SendChunkNotice(const ChunkId& id)
            : id{id}
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
            sender.notify(id);
        }
    };

    /**
     * An entry in the set of active peers. This class adds attributes to a
     * peer, manages the threads on which the peer executes, and provides a
     * higher-level API to the peer.
     */
    class PeerEntry final
    {
        class Impl final : public Sender, public PeerServer
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

            /// Queue of send-actions
            SendQ                  sendQ;
            /// Remote peer
            Peer                   peer;
            /// Value of this entry
            std::atomic<PeerValue> value;
            /// Completion service for executing the sending and receiving tasks
            Completer<void>        completer;
            /// Higher-level component used by this instance
            PeerEntryServer&       peerEntryServer;
            /// Sender of backlog data-chunks to the remote peer
            Backlogger             backlogger;
            /// Thread on which the sender of the backlog executes
            Thread                 backlogThread;

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

        public:
            /**
             * Constructs. Acts upon input from the remote peer by calling the
             * higher-level component.
             * @param[in] peer             Remote peer
             * @param[in] peerEntryServer  Higher-level component. *Must* exist
             *                             the duration of the constructed
             *                             instance.
             * @param[in] maxResideTime    Maximum residence time for
             *                             send-actions
             * @throw LogicError           First message from remote peer isn't
             *                             chunk-request
             * @throw RuntimeError         Instance couldn't be constructed
             */
            Impl(   Peer&            peer,
                    PeerEntryServer& peerEntryServer,
                    const TimeUnit&  maxResideTime)
                : sendQ{maxResideTime}
                , peer{peer}
                , value{0}
                , completer{}
                /*
                 * g++(1) 4.8.5 doesn't support "{}"-based initialization of
                 * references; clang(1) does.
                 */
                , peerEntryServer(peerEntryServer)
                , backlogger{}
                , backlogThread{}
            {}

            /// Prevents copy and move construction and assignment.
            Impl(const Impl& that) =delete;
            Impl(const Impl&& that) =delete;
            Impl& operator=(const Impl& rhs) =delete;
            Impl& operator=(const Impl&& rhs) =delete;

            inline InetSockAddr getRemoteAddr()
            {
                return peer.getRemoteAddr();
            }

            /**
             * Executes this instance. Doesn't return unless the remote peer
             * closes the connection or an exception is thrown. Intended to run
             * on its own thread.
             * @param[in] earliest  ID of earliest missing data-chunk. May be
             *                      invalid.
             * @exceptionsafety     Basic guarantee
             * @threadsafety        Unsafe
             * @see                 `ChunkId::operator bool()`
             */
            void operator()(const ChunkId& earliest)
            {
                if (earliest) {
                    peer.requestBacklog(earliest);
                }
                else {
                    LOG_INFO("Backlog won't be requested because "
                            "there's no oldest missing data-chunk");
                }
                completer.submit([this]{peer.runReceiver(*this);});
                completer.submit([this]{runSender();});
                completer.take().getResult(); // Will rethrow any task exception
            }

            void startBacklog(const ChunkId& earliest)
            {
                /*
                 * NB: The following is safe even if `backlogThread` was
                 * default-constructed or is associated with an active thread
                 * because assignment cancels the target thread.
                 */
                backlogThread = Thread{
                        peerEntryServer.getBacklogger(earliest, peer)};
            }

            bool shouldRequest(const ProdIndex& prodIndex)
            {
                return peerEntryServer.shouldRequest(prodIndex);
            }

            bool shouldRequest(const ChunkId& chunkId)
            {
                return peerEntryServer.shouldRequest(chunkId);
            }

            bool get(const ProdIndex& prodIndex, ProdInfo& prodInfo)
            {
                return peerEntryServer.get(prodIndex, prodInfo);
            }

            bool get(const ChunkId& chunkId, ActualChunk& chunk)
            {
                return peerEntryServer.get(chunkId, chunk);
            }

            RecvStatus receive(const ProdInfo& info)
            {
                return peerEntryServer.receive(info, peer.getRemoteAddr());
            }

            RecvStatus receive(LatentChunk& chunk)
            {
                return peerEntryServer.receive(chunk, peer.getRemoteAddr());
            }

            void notify(const ProdIndex& prodIndex)
            {
                peer.notify(prodIndex);
            }

            void notify(const ChunkId& chunkId)
            {
                // No need to include this chunk in the backlog
                backlogger.doNotNotifyOf(chunkId);
                peer.notify(chunkId);
            }

            /**
             * Adds a send-action to the send-action queue.
             * @param[in] action         Send-action to be added
             */
            void push(std::shared_ptr<SendAction> action)
            {
                sendQ.push(action);
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
        };

        std::shared_ptr<Impl> pImpl;

    public:
        inline PeerEntry()
            : pImpl{}
        {}
        inline PeerEntry(
                Peer&            peer,
                PeerEntryServer& peerEntryServer,
                const TimeUnit&  maxResideTime)
            : pImpl{new Impl(peer, peerEntryServer, maxResideTime)}
        {}
        inline InetSockAddr getRemoteAddr() const { return pImpl->getRemoteAddr(); }
        inline void operator()(const ChunkId& earliest)
                                            const { pImpl->operator()(earliest); }
        inline Peer getPeer()               const { return pImpl->getPeer(); }
        inline void incValue()              const { pImpl->incValue(); }
        inline void decValue()              const { pImpl->decValue(); }
        inline PeerValue getValue()         const { return pImpl->getValue(); }
        inline void resetValue()            const { pImpl->resetValue(); }
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
    const TimeUnit                              stasisDuration;
    const TimeUnit                              maxResideTime;
    unsigned                                    maxPeers;
    std::exception_ptr                          exception;
    TimePoint                                   timeLastInsert;
    PeerSetServer&                              peerSetServer;
    Completer<void>                             completer;

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
     * @pre                 `mutex` is locked
     * @param[in] peer      Peer to be inserted
     * @param[in] earliest  Id of earliest missing data-chunk
     * @post                `mutex` is locked
     * @exceptionsafety     Strong guarantee
     * @threadsafety        Compatible but not safe
     */
    void add(
            Peer&          peer,
            const ChunkId& earliest)
    {
        assert(isLocked(mutex));
        PeerEntry  entry{peer, *this, maxResideTime};
        PeerFuture peerFuture{};
        {
            UnlockGuard unlock{mutex};
            peerFuture = completer.submit([entry,earliest]() {
                entry.operator()(earliest);});
        }
        try {
            addrToEntryMap.insert({peer.getRemoteAddr(), entry});
            futureToEntryMap.insert({peerFuture, entry});
            timeLastInsert = Clock::now();
            if (full())
                resetValues();
        }
        catch (const std::exception& ex) {
            futureToEntryMap.erase(peerFuture);
            addrToEntryMap.erase(peer.getRemoteAddr());
            peerFuture.cancel(true);
        }
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

    void peerStopped(Future<void>& future)
    {
        InetSockAddr peerAddr{};
        {
            LockGuard                     lock{mutex};
            std::pair<bool, InetSockAddr> pair = erasePeer(future);
            if (pair.first)
                peerAddr = pair.second;
        }
        if (future.wasCanceled()) {
            LOG_INFO("Peer " + peerAddr.to_string() +
                    " was canceled");
        }
        else {
            try {
                try {
                    future.getResult();
                }
                catch (const std::exception& ex) {
                    std::throw_with_nested(RUNTIME_ERROR(
                            "Peer " + peerAddr.to_string() +
                            " threw an exception"));
                }
            }
            catch (const std::exception& ex) {
                log_warn(ex);
            }
        }
        if (peerAddr)
            peerSetServer.peerStopped(peerAddr);
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
                    peerStopped(future);
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

    /**
     * Notifies all remote peers, except one, about available information on a
     * product.
     * @param[in] prodIndex  Product index
     * @param[in] except     Address of remote peer to exclude
     */
    void notify(
            const ProdIndex&    prodIndex,
            const InetSockAddr& except)
    {
        LockGuard lock{mutex};
    	if (exception)
            std::rethrow_exception(exception);
        std::shared_ptr<SendProdNotice> action{new SendProdNotice(prodIndex)};
        for (const auto& elt : addrToEntryMap) {
            if (elt.first == except)
                continue;
            elt.second.push(action);
        }
    }

    /**
     * Notifies all remote peers, except one, about an available chunk-of-data.
     * @param[in] chunkId  Chunk ID
     * @param[in] except   Address of remote peer to exclude
     */
    void notify(
            const ChunkId&      id,
            const InetSockAddr& except)
    {
        LockGuard lock{mutex};
    	if (exception)
            std::rethrow_exception(exception);
        std::shared_ptr<SendChunkNotice> action{new SendChunkNotice(id)};
        for (const auto& elt : addrToEntryMap) {
            if (elt.first == except)
                continue;
            elt.second.push(action);
        }
    }

public:
    /**
     * Constructs from the maximum number of peers. The set will be empty.
     * @param[in] p2pServer       Higher-level peer-to-peer component
     * @param[in] maxPeers        Maximum number of peers
     * @param[in] stasisDuration  Minimum amount of time that the set must be
     *                            full and unchanged before the worst-performing
     *                            peer may be removed
     * @throws InvalidArgument    `maxPeers == 0 || stasisDuration <= 0`
     */
    Impl(   PeerSetServer& peerSetServer,
            const unsigned maxPeers,
            const unsigned stasisDuration)
        : mutex{}
        , cond{}
        , addrToEntryMap{}
        , futureToEntryMap{}
        , stasisDuration{stasisDuration}
        , maxResideTime{stasisDuration*2}
        , maxPeers{maxPeers}
        , exception{}
        , timeLastInsert{Clock::now()}
        /*
         * g++(1) 4.8.5 doesn't support "{}"-based initialization of references;
         * clang(1) does.
         */
        , peerSetServer(peerSetServer)
        , completer{}
    {
        if (maxPeers == 0)
            throw INVALID_ARGUMENT("Maximum number of peers can't be zero");
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
                auto earliest = peerSetServer.getEarliestMissingChunkId();
                add(peer, earliest);
                inserted = true;
            }
        } // `mutex` locked
#if 0
        if (peerAddr)
            peerSetServer.peerStopped(peerAddr);
#endif
        return inserted;
    }

    bool contains(const InetSockAddr& peerAddr) const
    {
        LockGuard lock{mutex};
    	if (exception)
            std::rethrow_exception(exception);
        return addrToEntryMap.find(peerAddr) != addrToEntryMap.end();
    }

    void notify(const ProdIndex& prodIndex)
    {
        LockGuard lock{mutex};
    	if (exception)
            std::rethrow_exception(exception);
        std::shared_ptr<SendProdNotice> action{new SendProdNotice(prodIndex)};
        for (const auto& elt : addrToEntryMap)
            elt.second.push(action);
    }

    void notify(const ChunkId& id)
    {
        LockGuard lock{mutex};
    	if (exception)
            std::rethrow_exception(exception);
        std::shared_ptr<SendChunkNotice> action{new SendChunkNotice(id)};
        for (const auto& elt : addrToEntryMap)
            elt.second.push(action);
    }

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

    size_t size() const
    {
        LockGuard lock{mutex};
    	if (exception)
            std::rethrow_exception(exception);
        return addrToEntryMap.size();
    }

    bool isFull() const
    {
        return size() >= maxPeers;
    }

    Backlogger getBacklogger(const ChunkId& chunkId, Peer& peer)
    {
        return peerSetServer.getBacklogger(chunkId, peer);
    }

    bool shouldRequest(const ProdIndex& prodIndex)
    {
        return peerSetServer.shouldRequest(prodIndex);
    }

    bool shouldRequest(const ChunkId& chunkId)
    {
        return peerSetServer.shouldRequest(chunkId);
    }

    bool get(const ProdIndex& prodIndex, ProdInfo& prodInfo)
    {
        return peerSetServer.get(prodIndex, prodInfo);
    }

    bool get(const ChunkId& chunkId, ActualChunk& chunk)
    {
        return peerSetServer.get(chunkId, chunk);
    }

    RecvStatus receive(const ProdInfo& info, const InetSockAddr& peerAddr)
    {
        auto status = peerSetServer.receive(info, peerAddr);
        if (status.isNew())
            notify(info.getIndex(), peerAddr);
        return status;
    }

    RecvStatus receive(LatentChunk& chunk, const InetSockAddr& peerAddr)
    {
        auto status = peerSetServer.receive(chunk, peerAddr);
        if (status.isNew())
            notify(chunk.getId(), peerAddr);
        return status;
    }
}; // `PeerSet::Impl`

PeerSet::PeerSet(
        PeerSetServer& peerSetServer,
        const unsigned maxPeers,
        const unsigned stasisDuration)
    : pImpl(new Impl(peerSetServer, maxPeers, stasisDuration))
{}

bool PeerSet::tryInsert(Peer& peer) const
{
    return pImpl->tryInsert(peer);
}

bool PeerSet::contains(const InetSockAddr& peerAddr) const
{
    return pImpl->contains(peerAddr);
}

void PeerSet::notify(const ProdIndex& prodIndex) const
{
    pImpl->notify(prodIndex);
}

void PeerSet::notify(const ChunkId& chunkId) const
{
    pImpl->notify(chunkId);
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
