/**
 * This file implements a local manager of a peer-to-peer network.
 *
 *  @file:  P2pMgr.cpp
 * @author: Steven R. Emmerson <emmerson@ucar.edu>
 *
 *    Copyright 2021 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "config.h"

#include "P2pMgr.h"

#include "Bookkeeper.h"
#include "Node.h"
#include "ThreadException.h"

#include <condition_variable>
#include <exception>
#include <list>
#include <pthread.h>
#include <set>
#include <system_error>
#include <unordered_map>

namespace hycast {

/**************************************************************************************************/

/// Base P2P manager implementation
class P2pMgrImpl : public P2pMgr
{
    Node&             node;          ///< Hycast node
    bool              peersChanged;  ///< Set of peers has changed?

    template<typename ID>
    void notify(const ID id) {
        Guard guard{peerSetMutex};
        for (auto iter = peerSet.begin(), stop = peerSet.end(); iter != stop; ) {
            auto peer = *(iter++); // Increment prevents becoming invalid
            if (shouldNotify(peer, id)) {
                if (!peer->notify(id))
                    lockedRemove(peer);
            }
        }
    }

protected:
    using PeerSet = std::set<Peer::Pimpl>;
    using PeerMap = std::unordered_map<SockAddr, Peer::Pimpl>;

    enum class State {
        INIT,
        STARTED,
        STOPPING,
        STOPPED
    }                    state;         ///< State of this instance
    mutable Mutex        stateMutex;    ///< For changing state
    mutable Cond         stateCond;     ///< For changing state
    mutable Mutex        peerSetMutex;  ///< To protect the set of active peers
    mutable Cond         peerSetCond;   ///< To support concurrency
    Tracker              tracker;       ///< Socket addresses of available but unused P2P servers
    ThreadEx             threadEx;      ///< Exception thrown by internal threads
    const unsigned       maxSrvrPeers;  ///< Maximum number of server-side peers
    unsigned             numSrvrPeers;  ///< Number of server-side peers
    PeerSet              peerSet;       ///< Set of peers
    PeerMap              peerMap;       ///< Map from remote address to peer
    Bookkeeper::Pimpl    bookkeeper;    ///< Monitors peer performance
    Thread               acceptThread;  ///< Creates server-side peers
    Thread               improveThread; ///< Improves the set of peers
    std::chrono::seconds evalTime;      ///< Evaluation interval for poorest-performing peer

    /**
     * @pre               The state mutex is locked
     * @post              The state is STARTED
     * @throw LogicError  Instance can't be re-executed
     */
    void startImpl() {
        if (state != State::INIT)
            throw LOGIC_ERROR("Instance can't be re-executed");
        startThreads();
        state = State::STARTED;
    }

    /**
     * Idempotent.
     *
     * @pre  The state mutex is locked
     * @post The state is STOPPED
     */
    void stopImpl() {
        /*
         * Ensure that the set of active peers will no longer be changed so that it can be iterated
         * through without locking it, which can deadlock with `SubP2pMgr::recvData()`.
         */
        stopThreads();
        // Ensure the latest view of the set of active peers now that it won't change.
        { Guard guard{peerSetMutex}; }
        for (auto peer : peerSet) {
            peer->stop();
            bookkeeper->erase(peer); // Necessary to prevent P2pMgr_test hanging for some reason
        }
        state = State::STOPPED;
        threadEx.throwIfSet();
    }

    void setException(const std::exception& ex) {
        Guard guard{stateMutex};
        threadEx.set(ex);
        state = State::STOPPING;
        stateCond.notify_one();
    }

    /**
     * Handles a peer being added to the set of active peers. Sends to the remote peer
     *   - An indicator of whether or not the local P2P node has a path to the publisher; and
     *   - The addresses of known, potential P2P servers.
     *
     * @param[in] peer     Peer whose remote counterpart is to be notified.
     * @retval    `true`   Success
     * @retval    `false`  Lost connection
     */
    virtual bool peerAdded(Peer::Pimpl peer) =0;

    /**
     * Adds a peer. Adds the peer to the set of active peers and the bookkeeper; then starts the
     * peer. Notifies waiting threads about the change.
     *
     * @pre                   Mutex is unlocked
     * @param[in] peer        Peer to be added
     * @retval    `true`      Peer added
     * @retval    `false`     Peer was previously added
     * @post                  Mutex is unlocked
     */
    bool add(Peer::Pimpl peer) {
        {
            Guard guard{peerSetMutex};
            if (!peerSet.insert(peer).second)
                return false;

            peerMap[peer->getRmtAddr()] = peer;
            const auto added = bookkeeper->add(peer);
            LOG_ASSERT(added);
            incPeerCount(peer);
            peersChanged = true;
            peerSetCond.notify_all();
        }

        peer->start();
        return true;
    }

    /**
     * Handles a peer being removed from the set of active peers. Does nothing by default because a
     * publishing P2P manager doesn't initiate connections to remote P2P servers.
     *
     * @pre             Peer set mutex is locked
     * @param[in] peer  The peer that was removed
     * @post            Peer set mutex is locked
     */
    virtual void peerRemoved(Peer::Pimpl peer) {
    }

    /**
     * Removes a peer.
     *   - Stops the peer;
     *   - Removes it from the set of active peers;
     *   - Removes it from the bookkeeper;
     *   - Decrements the count of peers;
     *   - Calls the peer-removed cleanup function; and
     *   - Notifies waiting threads that the set of peers has changed.
     *
     * @pre              Set-of-peers mutex is locked
     * @param[in] peer   Peer to be removed
     * @post             Set-of-peers mutex is locked
     * @see 'decPeerCount()`
     * @see 'peerRemoved()`
     */
    void lockedRemove(Peer::Pimpl peer) {
        LOG_ASSERT(!peerSetMutex.try_lock());

        peer->stop();

        if (peerSet.erase(peer)) {
            const auto n = peerMap.erase(peer->getRmtAddr());
            LOG_ASSERT(n);
            const auto existed = bookkeeper->erase(peer);
            LOG_ASSERT(existed);
            decPeerCount(peer);
            peersChanged = true;
            peerSetCond.notify_all();
            peerRemoved(peer);
        }
    }

    /**
     * Removes a peer.
     *   - Locks the peer-set mutex;
     *   - Calls `lockedRemove()`; and
     *   - Unlocks the peer-set mutex.
     *
     * @pre              Set-of-peers mutex is unlocked
     * @param[in] peer   Peer to be removed
     * @post             Set-of-peers mutex is unlocked
     * @see `lockedRemove()`
     */
    void remove(Peer::Pimpl peer) {
        Guard guard{peerSetMutex};
        lockedRemove(peer);
    }

    /**
     * Returns the server-side peer corresponding to an accepted connection from a client-side peer.
     * The returned peer is connected to its remote counterpart but not yet receiving from it.
     *
     * @return  Server-side peer
     */
    virtual Peer::Pimpl accept() =0;

    void cancelAndJoin(Thread& thread) {
        if (thread.joinable()) {
            auto status = ::pthread_cancel(thread.native_handle());
            if (status)
                throw SYSTEM_ERROR("pthread_cancel() failure");

            try {
                thread.join();
            }
            catch (const std::exception& ex) {
                std::throw_with_nested(RUNTIME_ERROR("thread::join() failure"));
            }
        }
    }

    /**
     * Starts the internal threads.
     */
    virtual void startThreads() =0;

    /**
     * Stops the internal threads.
     */
    virtual void stopThreads() =0;

    /**
     * Processes addition of a peer by incrementing the count of that type of
     * peer.
     *
     * @pre                Mutex is locked
     * @param[in] peer     Peer that was added
     * @post               Mutex is locked
     */
    virtual void incPeerCount(Peer::Pimpl peer) =0;

    /**
     * Processes removal of a peer by decrementing the count of that type of
     * peer.
     *
     * @pre             Peer set mutex is locked
     * @param[in] peer  Peer to be removed
     * @post            Peer set mutex is locked
     */
    virtual void decPeerCount(Peer::Pimpl peer) =0;

    /**
     * Indicates if a remote peer should be notified about available information
     * on a product.
     *
     * @pre                  Peer-set mutex is locked
     * @param[in] peer       Peer
     * @param[in] prodId     Product identifier
     * @retval    `true`     Peer should be notified
     * @retval    `false`    Peer should not be notified
     * @post                 Peer-set mutex is locked
     */
    virtual bool shouldNotify(
            Peer::Pimpl peer,
            ProdId      prodId) =0;

    /**
     * Indicates if a remote peer should be notified about an available data
     * segment.
     *
     * @pre                  Peer-set mutex is locked
     * @param[in] peer       Peer
     * @param[in] dataSegId  ID of the data segment
     * @retval    `true`     Peer should be notified
     * @retval    `false`    Peer should not be notified
     * @post                 Peer-set mutex is locked
     */
    virtual bool shouldNotify(
            Peer::Pimpl peer,
            DataSegId   dataSegId) =0;

public:
    /**
     * Constructs.
     *
     * @param[in]     node          ///< Associated Hycast node
     * @param[in,out] tracker       ///< Tracks available but unused P2P servers
     * @param[in]     maxPeers      ///< Maximum number of peers
     * @param[in]     maxSrvrPeers  ///< Maximum number of server-side-constructed peers
     * @param[in]     evalTime      ///< Peer evaluation time in seconds
     */
    P2pMgrImpl(
            Node&          node,
            Tracker        tracker,
            const unsigned maxPeers,
            const unsigned maxSrvrPeers,
            const unsigned evalTime)
        : state(State::INIT)
        , stateMutex()
        , stateCond()
        , node(node)
        , peersChanged(false)
        , peerSetMutex()
        , peerSetCond()
        , tracker(tracker)
        , threadEx()
        , maxSrvrPeers(maxSrvrPeers)
        , numSrvrPeers(0)
        , peerSet()
        , peerMap(maxPeers)
        , bookkeeper()
        , acceptThread()
        , improveThread()
        , evalTime(evalTime)
    {
        if (maxSrvrPeers <= 0)
            throw INVALID_ARGUMENT("Invalid maximum number of server-side peers: " +
                    std::to_string(maxSrvrPeers));
    }

    virtual ~P2pMgrImpl() noexcept {
        //LOG_DEBUG("Called");
        //LOG_DEBUG("Returning");
    }

    void start() override {
        Guard guard{stateMutex};
        startImpl();
    }

    /**
     * This function is necessary for the caller to catch an exception thrown by an internal thread.
     */
    void stop() override {
        Guard guard{stateMutex};
        if (state == State::STARTED)
            stopImpl();
    }

    void run() override {
        Lock lock{stateMutex};
        startImpl();
        stateCond.wait(lock, [&]{return state == State::STOPPING;});
        stopImpl();
    }

    void halt() override {
        Guard guard{stateMutex};
        if (state == State::STARTED) {
            state = State::STOPPING;
            stateCond.notify_one();
        }
    }

    /**
     * Runs the P2P server. Accepts connecting remote peers and adds them to the set of active
     * server-side peers. Must be public because it's the start function for a thread.
     *
     * NB: `noexcept` is incompatible with thread cancellation.
     */
    void acceptPeers() {
        try {
            auto needPeer = [&]{return numSrvrPeers < maxSrvrPeers;};
            for (;;) {
                {
                    Lock lock{peerSetMutex};
                    peerSetCond.wait(lock, needPeer);
                    /*
                     * The mutex is released immediately because  a server-side peer is only added
                     * by the current thread.
                     */
                }

                auto peer = accept();
                if (!add(peer)) {
                    LOG_WARNING("Peer %s was previously accepted", peer->to_string().data());
                }
                else {
                    LOG_NOTE("Accepted connection from peer %s", peer->to_string().data());
                    if (!peerAdded(peer))
                        remove(peer);
                }
            }
        }
        catch (const std::exception& ex) {
            try {
                std::throw_with_nested(
                        RUNTIME_ERROR("PeerSrvr::accept() failure"));
            }
            catch (const std::exception& ex) {
                setException(ex);
            }
        }
    }

    /**
     * Improves the set of peers by periodically removing the worst-performing peer and notifying
     * the peer-adding threads.
     *
     * Must be public so that it's visible to the associated thread object.
     *
     * @param[in] numPeers    Number of relevant peers
     * @param[in] maxPeers    Maximum number of relevant peers
     */
    void improvePeers(
            unsigned&      numPeers,
            const unsigned maxPeers) {
        try {
            Lock lock{peerSetMutex};
            auto resetNeeded = [&]{return peersChanged;};

            for (;;) {
                bookkeeper->reset();
                peersChanged = false;

                // Wait until the set of relevant peers is unchanged for the evaluation duration
                if (!peerSetCond.wait_for(lock, evalTime, resetNeeded) && (numPeers >= maxPeers)) {
                    auto worstPeer = bookkeeper->getWorstPeer();
                    if (worstPeer)
                        lockedRemove(worstPeer);
                }
            }
        }
        catch (const std::exception& ex) {
            setException(ex);
        }
    }

    void waitForSrvrPeer() override {
        Lock lock{peerSetMutex};
        peerSetCond.wait(lock, [&]{return numSrvrPeers > 0;});
    }

    void notify(const ProdId prodId) override {
        LOG_DEBUG("Notifying peers about product %s", prodId.to_string().data());
        notify<ProdId>(prodId);
    }

    void notify(const DataSegId segId) override {
        LOG_DEBUG("Notifying peers about data-segment %s", segId.to_string().data());
        notify<DataSegId>(segId);
    }

    ProdIdSet::Pimpl subtract(ProdIdSet::Pimpl other) const override {
        return node.subtract(other);
    }

    ProdIdSet::Pimpl getProdIds() const override {
        return node.getProdIds();
    }

    ProdInfo getDatum(const ProdId prodId) {
        return node.recvRequest(prodId);
    }

    DataSeg getDatum(const DataSegId segId) {
        return node.recvRequest(segId);
    }

    void recvHavePubPath(
            const bool     amPubPath,
            const SockAddr rmtAddr) override {
        /*
         * To prevent an orphan network, terminate client-side peers whose remote counterpart
         * doesn't provide a path to the publisher.
         */
        if (!amPubPath) {
            Guard guard{peerSetMutex};
            if (peerMap.count(rmtAddr)) {
                auto& peer = peerMap[rmtAddr];
                if (peer->isClient()) {
                    LOG_DEBUG("Disconnecting from P2P node " + peer->getRmtAddr().to_string() +
                            " because it isn't a path to the publisher");
                    lockedRemove(peer);
                }
            }
        }
    }

    void recvAdd(const SockAddr p2pSrvrAddr) override {
        if (p2pSrvrAddr != getSrvrAddr()) // Connecting to oneself is useless
            this->tracker.insert(p2pSrvrAddr);
    }

    void recvAdd(Tracker tracker) override {
        tracker.erase(getSrvrAddr()); // Connecting to oneself is useless
        this->tracker.insert(tracker);
    }

    void recvRemove(const SockAddr p2pSrvrAddr) override {
        this->tracker.erase(p2pSrvrAddr);
    }
    void recvRemove(const Tracker tracker) override {
        this->tracker.erase(tracker);
    }
};

/**************************************************************************************************/

/// Publisher's P2P manager implementation
class PubP2pMgrImpl final : public P2pMgrImpl
{
    PubP2pSrvr::Pimpl p2pSrvr;   ///< P2P server

    void stopThreadsImpl() {
        cancelAndJoin(improveThread);
        cancelAndJoin(acceptThread);
    }

protected:
    Peer::Pimpl accept() override {
        return p2pSrvr->accept(*this);
    }

    void startThreads() override {
        try {
            acceptThread = Thread(&P2pMgrImpl::acceptPeers, this);
            improveThread = Thread(&P2pMgrImpl::improvePeers, this,
                    std::ref(numSrvrPeers), maxSrvrPeers);
        }
        catch (const std::exception& ex) {
            stopThreads();
            throw;
        }
    }

    void stopThreads() override {
        stopThreadsImpl();
    }

    bool peerAdded(Peer::Pimpl peer) override {
        /*
         * The remote peer knows that this instance is a publisher's; therefore, there's no need to
         * inform it that this instance is a path to the publisher.
         *
         * `getPeerSetTracker()` is useless because all those peers were constructed server-side,
         * which means that their remote addresses aren't their P2P servers'.
         */
        return peer->add(tracker);
    }

    void incPeerCount(Peer::Pimpl peer) override {
        LOG_ASSERT(!peerSetMutex.try_lock());
        ++numSrvrPeers;
    }

    void decPeerCount(Peer::Pimpl peer) override {
        LOG_ASSERT(!peerSetMutex.try_lock());
        --numSrvrPeers;
    }

    bool shouldNotify(
            Peer::Pimpl peer,
            ProdId      prodId) override {
        LOG_ASSERT(!peerSetMutex.try_lock());
        // Publisher's peer should always notify the remote peer
        return true;
    }

    bool shouldNotify(
            Peer::Pimpl      peer,
            DataSegId dataSegId) override {
        LOG_ASSERT(!peerSetMutex.try_lock());
        // Publisher's peer should always notify the remote peer
        return true;
    }

public:
    using P2pMgrImpl::waitForSrvrPeer;
    using P2pMgrImpl::getDatum;

    /**
     * Constructs.
     *
     * @param[in] pubNode       Publisher's node
     * @param[in] p2pSrvr       P2P server
     * @param[in] maxPeers      Maximum number of peers
     * @param[in] evalTime      Evaluation time for poorest-performing peer in seconds
     */
    PubP2pMgrImpl(PubNode&                 pubNode,
                  const PubP2pSrvr::Pimpl  p2pSrvr,
                  const unsigned           maxPeers,
                  const unsigned           evalTime)
        : P2pMgrImpl(pubNode, Tracker{}, maxPeers, maxPeers, evalTime)
        , p2pSrvr(p2pSrvr)
    {
        bookkeeper = Bookkeeper::createPub(maxPeers);
    }

    PubP2pMgrImpl(const PubP2pMgrImpl& impl) =delete;

    ~PubP2pMgrImpl() noexcept {
        try {
            stop();
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex, "Couldn't stop execution");
        }
    }

    PubP2pMgrImpl& operator=(const PubP2pMgrImpl& rhs) =delete;

    SockAddr getSrvrAddr() const override {
        return p2pSrvr->getSrvrAddr();
    }

    void notify(const ProdId prodId) override {
        P2pMgrImpl::notify(prodId);
    }

    void notify(const DataSegId segId) override {
        P2pMgrImpl::notify(segId);
    }

    ProdInfo getDatum(
            const ProdId   prodId,
            const SockAddr rmtAddr) override {
        {
            Guard guard{peerSetMutex};
            if (peerMap.count(rmtAddr) == 0)
                return ProdInfo{};
            bookkeeper->requested(peerMap.at(rmtAddr));
        }
        return getDatum(prodId);
    }

    DataSeg getDatum(
            const DataSegId segId,
            const SockAddr  rmtAddr) override {
        {
            Guard guard{peerSetMutex};
            if (peerMap.count(rmtAddr) == 0)
                return DataSeg{};
            bookkeeper->requested(peerMap.at(rmtAddr));
        }
        return getDatum(segId);
    }
};

P2pMgr::Pimpl P2pMgr::create(
        PubNode&       pubNode,
        const SockAddr peerSrvrAddr,
        const unsigned maxPeers,
        const unsigned listenSize,
        const unsigned evalTime) {
    auto peerSrvr = PubP2pSrvr::create(peerSrvrAddr, listenSize);
    return Pimpl{new PubP2pMgrImpl(pubNode, peerSrvr, maxPeers, evalTime)};
}

/**************************************************************************************************/

/// Subscribing P2P manager implementation
class SubP2pMgrImpl final : public SubP2pMgr, public P2pMgrImpl
{
    /**
     * Thread-safe delay FIFO of addresses of unused P2P servers.
     */
    class DelayFifo
    {
        using Clock = std::chrono::steady_clock;

        struct Entry {
            using TimePoint = std::chrono::time_point<Clock>;

        public:
            SockAddr      sockAddr;   ///< Socket address of P2P server
            TimePoint     revealTime; ///< When should entry become visible

            Entry(const SockAddr& sockAddr)
                : sockAddr(sockAddr)
                // TODO: Make delay-time user-configurable
                , revealTime(Clock::now() + std::chrono::seconds(60))
            {}
        };

        mutable Mutex    mutex;
        mutable Cond     cond;
        const size_t     capacity;
        std::list<Entry> fifo;

    public:
        /**
         * Constructs. The FIFO will be empty.
         *
         * @param[in] capacity  Capacity in socket addresses.
         */
        // TODO: Make capacity user-configurable
        explicit DelayFifo(const size_t capacity = 100)
            : mutex()
            , cond()
            , capacity(capacity)
            , fifo()
        {}

        /**
         * Adds the address of a P2P server to the FIFO. If the capacity
         * is exceeded, then the oldest entries are deleted. The address will
         * not be returned by `removeNext()` until its delay has occurred.
         *
         * @param[in] srvrAddr  Address of P2P server
         * @exceptionsafety     Strong guarantee
         * @threadsafety        Safe
         * @see                 `removeNext()`
         */
        void insert(const SockAddr& srvrAddr) {
            Guard guard(mutex);
            fifo.push_back(Entry(srvrAddr));
            while (fifo.size() > capacity)
                fifo.pop_front();
            cond.notify_all();
        }

        /**
         * Removes and returns the oldest P2P server address. Blocks until one
         * is available.
         *
         * @return        Socket address of server
         * @threadsafety  Safe
         * @see           `insert()`
         */
        SockAddr removeNext() {
            Lock lock(mutex);
            cond.wait(lock, [&]{return !fifo.empty() &&
                Clock::now() >= fifo.front().revealTime;});

            SockAddr sockAddr = fifo.front().sockAddr;
            fifo.pop_front();

            return sockAddr;
        }
    };

    DelayFifo          delayFifo;     ///< Delay FIFO of unused P2P servers
    SubNode&           subNode;       ///< Subscriber's node
    const unsigned     maxClntPeers;  ///< Maximum number of client-side peers
    unsigned           numClntPeers;  ///< Number of client-side peers
    int                timeout;       ///< Timeout, in ms, for connecting to remote P2P server
    SubP2pSrvr::Pimpl  p2pSrvr;       ///< Subscriber's P2P server
    Thread             retryThread;   ///< Moves peer-server addresses into tracker
    Thread             connectThread; ///< Creates client-side peers

    /**
     * Moves addresses of peer servers from the delay FIFO to the tracker so that they can be
     * retried by this instance.
     */
    void retry() {
        try {
            for (;;) {
                auto sockAddr = delayFifo.removeNext(); // Blocks until ready
                tracker.insert(sockAddr);
            }
        }
        catch (const std::exception& ex) {
            setException(ex);
        }
    }

    /**
     * Indicates whether or not this P2P node is a path to the publisher for a remote P2P node.
     *
     * @param[in] forPeer  Peer whose remote counterpart is to be ignored in the determination
     * @retval    `true`   Yes, this P2P node is a path to the publisher for the remote node
     * @retval    `false`  No, this P2P node is not a path to the publisher for the remote node
     */
    bool isPathToPubFor(Peer::Pimpl forPeer) {
        Guard guard{peerSetMutex};
        bool  isPathToPub = false;

        for (auto& peer : peerSet) {
            if (*peer != *forPeer) {
                if (peer->isRmtPathToPub()) {
                    isPathToPub = true;
                    break;
                }
            }
        }

        return isPathToPub;
    }

    /**
     * NB: `noexcept` is incompatible with thread cancellation.
     */
    void connectPeers() {
        try {
            auto needPeer = [&]{return numClntPeers < maxClntPeers;};
            for (;;) {
                {
                    Lock lock{peerSetMutex};
                    peerSetCond.wait(lock, needPeer);
                    /*
                     * The peer-set mutex is released immediately because a client-side peer is only
                     * added by the current thread.
                     */
                }

                auto rmtSrvrAddr = tracker.removeHead(); // Blocks if empty
                try {
                    auto peer = Peer::create(*this, rmtSrvrAddr);
                    try {
                        if (!add(peer))
                            throw LOGIC_ERROR("Already connected to " + rmtSrvrAddr.to_string());

                        try {
                            LOG_NOTE("Connected to %s", rmtSrvrAddr.to_string().data());
                            if (!peerAdded(peer))
                                remove(peer);
                        } // Peer was added to peer-set
                        catch (const std::exception& ex) {
                            remove(peer);
                            throw;
                        }
                    } // `peer` is connected
                    catch (const SystemError& ex) {
                        auto errnum = ex.code().value();
                        if (    errnum == ENETUNREACH  ||
                                errnum == ETIMEDOUT    ||
                                errnum == ECONNRESET   ||
                                errnum == EHOSTUNREACH ||
                                errnum == ENETDOWN     ||
                                errnum == EPIPE) {
                            // Peer is currently unavailable
                            LOG_NOTE(ex);
                            delayFifo.insert(rmtSrvrAddr);
                        }
                        else {
                            throw;
                        }
                    }
                } // `rmtSrvrAddr` removed from tracker
                catch (...) {
                    // The exception is unrelated to peer availability
                    tracker.insert(rmtSrvrAddr);
                    throw; // A thread cancellation "exception" *must* be rethrown
                }
            }
        }
        catch (const std::exception& ex) {
            setException(ex);
        }
    }

    Tracker getPeerSetTracker() {
        Guard   guard{peerSetMutex};
        Tracker tracker{peerSet.size()};
        for (auto peer : peerSet) {
            /*
             * A server-side peer's remote socket address isn't the remote P2P server's address.
             * Getting that address is handled by client-side peer calling `add(p2pSrvrAddr)`.
             */
            if (peer->isClient())
                tracker.insert(peer->getRmtAddr());
        }
        return tracker;
    }

protected:
    Peer::Pimpl accept() override {
        return p2pSrvr->accept(*this);
    }

    void startThreads() override {
        try {
            retryThread = Thread(&SubP2pMgrImpl::retry, this);
            connectThread = Thread(&SubP2pMgrImpl::connectPeers, this);
            acceptThread = Thread(&P2pMgrImpl::acceptPeers, this);
            improveThread = Thread(&SubP2pMgrImpl::improvePeers, this, std::ref(numClntPeers),
                    maxClntPeers);
        }
        catch (const std::exception& ex) {
            stopThreads();
            throw;
        }
    }

    void stopThreads() override {
        cancelAndJoin(improveThread);
        cancelAndJoin(acceptThread);
        cancelAndJoin(connectThread);
        cancelAndJoin(retryThread);
    }

    void incPeerCount(Peer::Pimpl peer) override {
        LOG_ASSERT(!peerSetMutex.try_lock());
        peer->isClient() ? ++numClntPeers : ++numSrvrPeers;
    }

    void decPeerCount(Peer::Pimpl peer) override {
        LOG_ASSERT(!peerSetMutex.try_lock());
        peer->isClient() ? --numClntPeers : --numSrvrPeers;
    }

    bool peerAdded(Peer::Pimpl peer) override {
        return peer->notifyHavePubPath(isPathToPubFor(peer)) &&
                peer->add(tracker) &&
                peer->add(getPeerSetTracker()) &&
                (!peer->isClient() || peer->add(p2pSrvr->getSrvrAddr()));
    }

    void peerRemoved(Peer::Pimpl peer) override {
        LOG_ASSERT(!peerSetMutex.try_lock());
        // Remote socket address is remote P2P server's only for a peer constructed client-side
        if (peer->isClient())
            delayFifo.insert(peer->getRmtAddr());
    }

    bool shouldNotify(
            Peer::Pimpl  peer,
            const ProdId prodId) override {
        LOG_ASSERT(!peerSetMutex.try_lock());
        // Remote peer is subscriber & doesn't have the datum => notify
        return !peer->isRmtPub() && bookkeeper->shouldNotify(peer, prodId);
    }

    bool shouldNotify(
            Peer::Pimpl     peer,
            const DataSegId dataSegId) override {
        LOG_ASSERT(!peerSetMutex.try_lock());
        // Remote peer is subscriber & doesn't have the datum => notify
        return !peer->isRmtPub() && bookkeeper->shouldNotify(peer, dataSegId);
    }

public:
    SubP2pMgrImpl(SubNode&           subNode,
                  Tracker            tracker,
                  SubP2pSrvr::Pimpl  p2pSrvr,
                  const int          timeout,
                  const unsigned     maxPeers,
                  const unsigned     evalTime)
        : P2pMgrImpl(subNode, tracker, ((maxPeers+1)/2)*2, (maxPeers+1)/2, evalTime)
        , delayFifo()
        , subNode(subNode)
        , maxClntPeers(maxSrvrPeers)
        , numClntPeers(0)
        , timeout(timeout)
        , p2pSrvr(p2pSrvr)
        , retryThread()
        , connectThread()
    {
        bookkeeper = Bookkeeper::createSub(maxPeers);
        // Ensure that this instance doesn't try to connect to itself
        tracker.erase(p2pSrvr->getSrvrAddr());
    }

    SubP2pMgrImpl(const SubP2pMgrImpl& impl) =delete;

    ~SubP2pMgrImpl() noexcept {
        try {
            stop();
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex, "Couldn't stop execution");
        }
    }

    SubP2pMgrImpl& operator=(const SubP2pMgrImpl& rhs) =delete;

    void start() override {
        P2pMgrImpl::start();
    }

    void stop() override {
        P2pMgrImpl::stop();
    }

    void run() {
        P2pMgrImpl::run();
    }

    void halt() {
        P2pMgrImpl::halt();
    }

    SockAddr getSrvrAddr() const override {
        return p2pSrvr->getSrvrAddr();
    }

    void waitForSrvrPeer() override {
        P2pMgrImpl::waitForSrvrPeer();
    }

    void notify(const ProdId prodId) override {
        P2pMgrImpl::notify(prodId);
    }

    void notify(const DataSegId segId) override {
        P2pMgrImpl::notify(segId);
    }

    ProdIdSet::Pimpl subtract(ProdIdSet::Pimpl other) const override {
        return P2pMgrImpl::subtract(other);
    }

    ProdIdSet::Pimpl getProdIds() const override {
        return P2pMgrImpl::getProdIds();
    }

    bool recvNotice(const ProdId   prodId,
                    const SockAddr rmtAddr) {
        // Must not exist and not been previously requested
        if (subNode.shouldRequest(prodId)) {
            Guard guard{peerSetMutex};
            if (peerMap.count(rmtAddr))
                return bookkeeper->shouldRequest(peerMap[rmtAddr], prodId);
        }
        return false;
    }
    bool recvNotice(const DataSegId segId,
                    SockAddr        rmtAddr) {
        // Must not exist and not been previously requested
        if (subNode.shouldRequest(segId)) {
            Guard guard{peerSetMutex};
            if (peerMap.count(rmtAddr))
                return bookkeeper->shouldRequest(peerMap[rmtAddr], segId);
        }
        return false;
    }

    ProdInfo getDatum(
            const ProdId   prodId,
            const SockAddr rmtAddr) override {
        return P2pMgrImpl::getDatum(prodId);
    }

    DataSeg getDatum(
            const DataSegId segId,
            const SockAddr  rmtAddr) override {
        return P2pMgrImpl::getDatum(segId);
    }

    void missed(const ProdId prodId,
                SockAddr     rmtAddr) {
        // TODO
        throw LOGIC_ERROR("Not implemented yet");
    }
    void missed(const DataSegId dataSegId,
                SockAddr        rmtAddr) {
        // TODO
        throw LOGIC_ERROR("Not implemented yet");
    }

    template<class DATUM>
    void recvData(const DATUM datum,
                  SockAddr    rmtAddr) {
        const auto id = datum.getId();
        LOG_DEBUG("Locking peer-set mutex");
        Guard      guard{peerSetMutex};
        LOG_DEBUG("Peer-set mutex locked");

        if (peerMap.count(rmtAddr)) {
            auto& peer = peerMap[rmtAddr];
            /*
             * The bookkeeper is accessed in 3 phases:
             *   1) The given peer's rating is increased if the reception is valid;
             *   2) The bookkeeper is queried as to whether a peer should notify its remote
             *      counterpart about the received datum before the relevant information is deleted
             *      from the bookkeeper; and
             *   3) The relevant bookkeeper entries are deleted.
             */
            LOG_DEBUG("Calling bookkeeper");
            if (bookkeeper->received(peer, id)) {
                LOG_DEBUG("Calling subscribing node");
                subNode.recvP2pData(datum);

                LOG_DEBUG("Notifying peers");
                for (auto p : peerSet) {
                    if (shouldNotify(p, datum.getId()))
                        p->notify(id);
                }
                LOG_DEBUG("Erasing bookkeeper entry");
                bookkeeper->erase(id); // No longer relevant
            }
            else {
                LOG_DEBUG("Datum " + id.to_string() + " was unexpected");
            }
        }
        else {
            LOG_DEBUG("Remote P2P server " + rmtAddr.to_string() + " not found");
        }
    }
    void recvData(const ProdInfo prodInfo,
                  const SockAddr rmtAddr) {
        recvData<ProdInfo>(prodInfo, rmtAddr);
    }
    void recvData(const DataSeg  dataSeg,
                  const SockAddr rmtAddr) {
        recvData<DataSeg>(dataSeg, rmtAddr);
    }
};

/*
SubP2pMgr::Pimpl SubP2pMgr::create(
        SubNode&          subNode,
        Tracker           tracker,
        const TcpSrvrSock srvrSock,
        const int         acceptQSize,
        const unsigned    maxPeers,
        const unsigned    evalTime) {
    auto p2pSrvr = SubP2pSrvr::create(srvrSock, acceptQSize);
    return Pimpl{new SubP2pMgrImpl(subNode, tracker, p2pSrvr, maxPeers, evalTime)};
}
*/

SubP2pMgr::Pimpl SubP2pMgr::create(
        SubNode&          subNode,
        Tracker           tracker,
        const SockAddr    p2pSrvrAddr,
        const int         acceptQSize,
        const int         timeout,
        const unsigned    maxPeers,
        const unsigned    evalTime) {
    auto p2pSrvr = SubP2pSrvr::create(p2pSrvrAddr, acceptQSize);
    return Pimpl{new SubP2pMgrImpl(subNode, tracker, p2pSrvr, timeout, maxPeers, evalTime)};
}

} // namespace
