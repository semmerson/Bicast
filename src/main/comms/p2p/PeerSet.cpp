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
#include "ClntSctpSock.h"
#include "InetSockAddr.h"
#include "logging.h"
#include "MsgRcvr.h"
#include "Peer.h"
#include "PeerSet.h"
#include "ProdInfo.h"

#include <assert.h>
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
bool isLocked(std::mutex& mutex)
{
    if (!mutex.try_lock())
        return true;
    mutex.unlock();
    return false;
}

class PeerSet::Impl final
{
    typedef std::chrono::seconds                               TimeRes;
    typedef std::chrono::time_point<std::chrono::steady_clock> Time;
    typedef std::chrono::steady_clock                          Clock;

    /**
     * An abstract base class for send-actions to be performed on a peer.
     */
    class SendAction
    {
        Time whenCreated;
    public:
        SendAction()
            : whenCreated{Clock::now()}
        {}
        virtual ~SendAction() =default;
        Time getCreateTime() const
        {
            return whenCreated;
        }
        /**
         * Acts upon a peer.
         * @param[in,out] peer  Peer to be acted upon
         * @return `true`       Iff processing should continue
         */
        virtual void actUpon(Peer& peer) =0;
        virtual bool terminate() const
        {
            return false;
        }
    };

    /**
     * A send-action notice of a new product.
     */
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

    /**
     * A send-action notice of a new chunk-of-data.
     */
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

    /**
     * A send-action that terminates the peer.
     */
    class TerminatePeer final : public SendAction
    {
    public:
        void actUpon(Peer& peer)
        {}
        bool terminate() const
        {
            return true;
        }
    };

    /**
     * A queue of send-actions to be performed on a peer.
     */
    class SendQ final
    {
        std::queue<std::shared_ptr<SendAction>> queue;
        std::mutex                              mutex;
        std::condition_variable                 cond;
    public:
        SendQ()
            : queue{},
              mutex{},
              cond{}
              {}
        SendQ(const SendQ& that)
            : queue{that.queue}
        {}
        bool push(
                std::shared_ptr<SendAction> action,
                const TimeRes&              maxResideTime)
        {
            std::lock_guard<decltype(mutex)> lock{mutex};
            if (!queue.empty()) {
                if (maxResideTime <
                        (action->getCreateTime() - queue.front()->getCreateTime()))
                    return false;
            }
            queue.push(action);
            cond.notify_one();
            return true;
        }
        SendQ& operator =(const SendQ& q) =delete;
        std::shared_ptr<PeerSet::Impl::SendAction> pop()
        {
            std::unique_lock<decltype(mutex)> lock{mutex};
            while (queue.empty())
                cond.wait(lock);
            auto action = queue.front();
            queue.pop();
            cond.notify_one();
            return action;
        }
        /**
         * Clears the queue and adds an send-action that will terminate the
         * associated peer.
         */
        void terminate()
        {
            std::unique_lock<decltype(mutex)> lock{mutex};
            while (!queue.empty())
                queue.pop();
            queue.push(std::shared_ptr<SendAction>(new TerminatePeer()));
            cond.notify_one();
        }
    };

    /**
     * The entry for an active peer.
     */
    class PeerEntryImpl
    {
        SendQ                      sendQ;
        Peer                       peer;
        std::function<void(Peer&)> handleFailure;
        uint32_t                   value;
        std::thread                sendThread;
        std::thread                recvThread;

        /**
         * Processes send-actions queued-up for a peer. Doesn't return until
         * the sentinel send-action is seen or an exception is thrown.
         */
        void processSendQ()
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
                log_what(e); // Because end of thread
                handleFailure(peer);
            }
        }
        /**
         * Causes a peer to receive messages from its associated remote peer.
         * Doesn't return until the remote peer disconnects or an exception is
         * thrown.
         */
        void runReceiver()
        {
            try {
                peer.runReceiver();
            }
            catch (const std::exception& e) {
                log_what(e); // Because end of thread
                handleFailure(peer);
            }
        }

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
        ~PeerEntryImpl()
        {
            // Destructors must never throw an exception
            try {
                sendQ.terminate();
                // Receiving code can handle cancellation
                ::pthread_cancel(recvThread.native_handle());
                sendThread.join();
                recvThread.join();
            }
            catch (const std::exception& e) {
                log_what(e);
            }
        }
        /**
         * Prevents copy assignment and move assignment.
         */
        PeerEntryImpl& operator=(PeerEntryImpl& rhs) =delete;
        PeerEntryImpl& operator=(PeerEntryImpl&& rhs) =delete;
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
    std::function<void(Peer&)>          peerTerminated;
    unsigned                            maxPeers;

    /**
     * Indicates if insufficient time has passed to determine the
     * worst-performing peer.
     * @return `true`  Iff it's too soon
     */
    inline bool tooSoon()
    {
        assert(isLocked(mutex));
        return Clock::now() < whenEligible;
    }

    /**
     * Indicates if the set is full.
     * @return `true`  Iff set is full
     */
    inline bool full() const
    {
        assert(isLocked(mutex));
        return peerEntries.size() >= maxPeers;
    }

    /**
     * Indicates if this instance contains a remote peer with a given Internet
     * socket address.
     * @param[in] addr  Internet socket address
     * @return `true`   Iff this instance contains a remote peer with the given
     *                  Internet socket address
     */
    bool contains(const InetSockAddr& addr)
    {
        assert(isLocked(mutex));
        for (auto entry : peerEntries) {
            const Peer* peer = &entry.first;
            if (addr == peer->getRemoteAddr())
                return true;
        }
        return false;
    }

    /**
     * Unconditionally inserts a peer. The peer immediately starts receiving
     * messages from its associated remote peer and is ready to send messages.
     * If the inserted peer makes the set of peers full, then
     *   - All value counters are reset;
     *   - The eligibility timer is set.
     * @param[in] peer   Peer to be inserted
     * @exceptionsafety  Strong guarantee
     * @threadsafety     Compatible but not safe
     */
    void insert(Peer& peer)
    {
        assert(isLocked(mutex));
        if (peerEntries.find(peer) == peerEntries.end()) {
            PeerEntry entry(new PeerEntryImpl(peer,
                    [=](Peer& p) { handleFailure(p); }));
            peerEntries.emplace(peer, entry);
            if (full()) {
                resetValues();
                setWhenEligible();
            }
        }
    }
    /**
     * Sets the time-point when the worst performing peer may be removed from
     * the set of peers.
     */
    void setWhenEligible()
    {
        assert(isLocked(mutex));
        whenEligible = Clock::now() + eligibilityDuration;
    }
    /**
     * Unconditionally removes the worst performing peer from the set of peers.
     * @return           The removed peer
     * @exceptionsafety  Nothrow
     * @threadsafety     Compatible but not safe
     */
    Peer removeWorstPeer()
    {
        assert(isLocked(mutex));
        assert(peerEntries.size() > 0);
        Peer worstPeer{};
        auto minValue = PeerEntryImpl::VALUE_MAX;
        for (const auto& elt : peerEntries) {
            auto value = elt.second->getValue();
            if (value <= minValue) {
                minValue = value;
                worstPeer = elt.first;
            }
        }
        peerEntries.erase(worstPeer);
        return worstPeer;
    }
    /**
     * Resets the value-counts in the map from peer to value.
     * @exceptionsafety  Nothrow
     * @threadsafety     Compatible but not safe
     */
    void resetValues()
    {
        assert(isLocked(mutex));
        for (auto& elt : peerEntries)
            elt.second->resetValue();
    }
    /**
     * Handles failure of a peer.
     * @param[in] peer  The peer that failed
     */
    void handleFailure(Peer& peer)
    {
        {
            std::lock_guard<decltype(mutex)> lock{mutex};
            peerEntries.erase(peer);
        }
        peerTerminated(peer);
    }

public:
    /**
     * Constructs from the maximum number of peers. The set will be empty.
     * @param[in] peerTerminated      Function to call when a peer terminates
     * @param[in] maxPeers            Maximum number of peers
     * @param[in] stasisDuration      Required duration, in seconds, without
     *                                change to the set of peers before the
     *                                worst-performing peer may be replaced
     * @throws std::invalid_argument  `maxPeers == 0`
     */
    Impl(
            std::function<void(Peer&)> peerTerminated,
            const unsigned             maxPeers,
            const unsigned             stasisDuration)
        : peerEntries{}
        , mutex{}
        , whenEligible{}
        , eligibilityDuration{std::chrono::seconds{stasisDuration}}
        , maxResideTime{eligibilityDuration*2}
        , peerTerminated{peerTerminated}
        , maxPeers{maxPeers}
    {
        if (maxPeers == 0)
            throw std::invalid_argument("Maximum number of peers can't be zero");
    }
    /**
     * Tries to insert a peer.
     * @param[in]  candidate  Candidate peer
     * @param[out] replaced   Replaced, worst-performing peer
     * @return                Insertion status:
     *   - EXISTS    Peer is already member of set
     *   - SUCCESS   Success
     *   - REPLACED  Success. `*replaced` is set iff `replaced != nullptr`
     *   - FULL      Set is full and insufficient time to determine worst peer
     * @exceptionsafety       Strong guarantee
     * @threadsafety          Safe
     */
    PeerSet::InsertStatus tryInsert(
            Peer& candidate,
            Peer* replaced)
    {
        std::lock_guard<decltype(mutex)> lock{mutex};
        if (peerEntries.find(candidate) != peerEntries.end())
            return PeerSet::EXISTS; // Candidate peer is already a member
        if (!full()) {
            // Just add the candidate peer
            insert(candidate);
            return PeerSet::SUCCESS;
        }
        // Set is full
        if (tooSoon())
            return PeerSet::FULL;
        // Replace worst-performing peer
        Peer worst{removeWorstPeer()};
        if (replaced)
            *replaced = worst;
        insert(candidate);
        return PeerSet::REPLACED;
    }

    /**
     * Tries to insert a remote peer given its Internet socket address.
     * @param[in]  candidate   Candidate remote peer
     * @param[in,out] msgRcvr  Receiver of messages from the remote peer
     * @param[out] replaced    Replaced, worst-performing peer
     * @return                 Insertion status:
     *   - EXISTS    Peer is already member of set
     *   - SUCCESS   Success
     *   - REPLACED  Success. `*replaced` is set iff `replaced != nullptr`
     *   - FULL      Set is full and insufficient time to determine worst peer
     * @exceptionsafety       Strong guarantee
     * @threadsafety          Safe
     */
    PeerSet::InsertStatus tryInsert(
            const InetSockAddr& candidate,
            PeerMsgRcvr&        msgRcvr,
            Peer*               replaced)
    {
        {
            std::lock_guard<decltype(mutex)> lock{mutex};
            if (full() && tooSoon())
                return PeerSet::FULL;
            if (contains(candidate))
                return PeerSet::EXISTS;
        }
        ClntSctpSock sock(candidate, Peer::getNumStreams());
        Peer         peer(msgRcvr, sock);
        return tryInsert(peer, replaced);
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
        std::lock_guard<decltype(mutex)> lock{mutex};
        std::shared_ptr<SendProdNotice> action{new SendProdNotice(info)};
        for (auto& elt : peerEntries)
            elt.second->push(action, maxResideTime);
    }

    /**
     * Sends information about a product to the remote peers except for one.
     * @param[in] info            Product information
     * @param[in] except          Peer to exclude
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Safe
     */
    void sendNotice(const ProdInfo& info, const Peer& except)
    {
        std::lock_guard<decltype(mutex)> lock{mutex};
        std::shared_ptr<SendProdNotice> action{new SendProdNotice(info)};
        for (auto& elt : peerEntries) {
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
        std::lock_guard<decltype(mutex)> lock{mutex};
        std::shared_ptr<SendChunkNotice> action{new SendChunkNotice(info)};
        for (auto& elt : peerEntries)
            elt.second->push(action, maxResideTime);
    }

    /**
     * Sends information about a chunk-of-data to the remote peers except for
     * one.
     * @param[in] info            Chunk information
     * @param[in] except          Peer to exclude
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Safe
     */
    void sendNotice(const ChunkInfo& info, const Peer except)
    {
        std::lock_guard<decltype(mutex)> lock{mutex};
        std::shared_ptr<SendChunkNotice> action{new SendChunkNotice(info)};
        for (auto& elt : peerEntries) {
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
        std::lock_guard<decltype(mutex)> lock{mutex};
        if (full()) {
            auto iter = peerEntries.find(peer);
            if (iter != peerEntries.end())
                iter->second->incValue();
        }
    }

    /**
     * Indicates if this instance is full.
     * @exceptionsafety Strong
     * @threadsafety    Safe
     */
    bool isFull() const
    {
        std::lock_guard<decltype(mutex)> lock{mutex};
        return full();
    }
};

PeerSet::PeerSet(
        std::function<void(Peer&)> peerTerminated,
        const unsigned             maxPeers,
        const unsigned             stasisDuration)
    : pImpl(new Impl(peerTerminated, maxPeers, stasisDuration))
{}

PeerSet::InsertStatus PeerSet::tryInsert(
        Peer& candidate,
        Peer* replaced) const
{
    return pImpl->tryInsert(candidate, replaced);
}

PeerSet::InsertStatus PeerSet::tryInsert(
        const InetSockAddr& candidate,
        PeerMsgRcvr&        msgRcvr,
        Peer*               replaced)
{
    return pImpl->tryInsert(candidate, msgRcvr, replaced);
}

void PeerSet::sendNotice(const ProdInfo& prodInfo)
{
    pImpl->sendNotice(prodInfo);
}

void PeerSet::sendNotice(const ProdInfo& prodInfo, const Peer& except)
{
    pImpl->sendNotice(prodInfo, except);
}

void PeerSet::sendNotice(const ChunkInfo& chunkInfo)
{
    pImpl->sendNotice(chunkInfo);
}

void PeerSet::sendNotice(const ChunkInfo& chunkInfo, const Peer& except)
{
    pImpl->sendNotice(chunkInfo, except);
}

void PeerSet::incValue(Peer& peer) const
{
    pImpl->incValue(peer);
}

bool PeerSet::isFull() const
{
    return pImpl->isFull();
}

} // namespace
