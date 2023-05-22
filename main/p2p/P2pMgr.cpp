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
#include <semaphore.h>
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
        Guard guard{stateMutex};

        if (state != State::STOPPING) {
            for (auto iter = peerSet.begin(), stop = peerSet.end(); iter != stop; ) {
                auto peer = *(iter++); // Increment prevents becoming invalid
                if (shouldNotify(peer, id))
                    peer->notify(id);
            }
        }
    }

    bool addPeer(Peer::Pimpl peer) {
        Guard guard{stateMutex};

        if (state != State::STOPPING) {
            if (!peerSet.insert(peer).second)
                return false;

            peerMap[peer->getRmtAddr()] = peer;
            const auto added = bookkeeper->add(peer);
            LOG_ASSERT(added);
            incPeerCount(peer);
            peersChanged = true;
            stateCond.notify_all();
        }

        return true;
    }

    void removePeer(Peer::Pimpl peer) {
        Guard guard{stateMutex};

        if (state != State::STOPPING) {
            if (peerSet.erase(peer)) {
                const auto n = peerMap.erase(peer->getRmtAddr());
                LOG_ASSERT(n);
                const auto existed = bookkeeper->erase(peer);
                LOG_ASSERT(existed);
                decPeerCount(peer);
                peersChanged = true;
                stateCond.notify_all();
            }
        }
    }

protected:
    using NumPeers = P2pSrvrInfo::NumAvail;                    ///< Number of connected peers
    using PeerSet  = std::set<Peer::Pimpl>;                     ///< Set of active peers
    using PeerMap  = std::unordered_map<SockAddr, Peer::Pimpl>; ///< Lookup table of active peers

    /// State of an instance
    enum class State {
        INIT,
        STARTED,
        STOPPING,
        STOPPED
    }                    state;         ///< State of this instance
    mutable Mutex        stateMutex;    ///< Guards state
    mutable Cond         stateCond;     ///< For notifying observers of state on other threads
    mutable sem_t        stopSem;       ///< For async-signal-safe stopping
    Tracker              tracker;       ///< Information on P2P-servers
    ThreadEx             threadEx;      ///< Exception thrown by internal threads
    const int            maxSrvrPeers;  ///< Maximum number of server-side peers
    int                  numSrvrPeers;  ///< Number of server-side peers
    PeerSet              peerSet;       ///< Set of active peers
    PeerMap              peerMap;       ///< Map from remote address to peer
    Bookkeeper::Pimpl    bookkeeper;    ///< Monitors peer performance
    Thread               acceptThread;  ///< Creates server-side peers
    Thread               improveThread; ///< Improves the set of active peers
    std::chrono::seconds evalTime;      ///< Evaluation interval for poorest-performing peer
    P2pSrvrInfo          srvrInfo;      ///< Information on the local P2P-server

    /**
     * Sets the exception to be thrown.
     * @param[in] ex  The exception to be thrown
     */
    void setException(const std::exception& ex) {
        threadEx.set(ex);
        ::sem_post(&stopSem);
    }

    /**
     * @pre               The state mutex is unlocked
     * @pre               The state is INIT
     * @throw LogicError  The state isn't INIT
     * @post              The state is STARTED
     * @post              The state mutex is unlocked
     */
    void startImpl() {
        Guard guard{stateMutex};
        if (state != State::INIT)
            throw LOGIC_ERROR("Instance can't be re-executed");
        try {
            startThreads();
        }
        catch (const std::exception& ex) {
            state = State::STOPPING;
            stateCond.notify_all();
            throw;
        }
        state = State::STARTED;
    }

    /**
     * Idempotent.
     *
     * @pre               The state mutex is unlocked
     * @pre               The state is STARTED
     * @throw LogicError  The state is not STARTED
     * @post              The state is STOPPED
     * @post              The state mutex is unlocked
     */
    void stopImpl() {
        // Stop modifying the set of active peers
        stopThreads();

        // Stop the active peers
        Lock lock{stateMutex};
        for (auto peer : peerSet)
            peer->halt();
        stateCond.wait(lock, [&]{return peerSet.empty();});

        state = State::STOPPED;
    }

    /**
     * Ensures that the information on the local P2P-server is up-to-date.
     * @return Current information on the local P2P-server -- updated if necessary
     */
    virtual P2pSrvrInfo& updateSrvrInfo() =0;

    /**
     * Updates the tier number. Updates the minimum number of hops to the publisher if the given
     * number is less that the current number.
     * @param[in] tier  The tier number to consider
     */
    virtual void updateTier(const P2pSrvrInfo::Tier tier) =0;

    /**
     * Handles a peer being removed from the set of active peers.
     *
     * @param[in] peer  The peer that was removed from the set of active peers
     */
    virtual void peerRemoved(Peer::Pimpl peer) =0;

    /**
     * Reassigns unsatisfied requests for data from a peer to other peers. Should be executed only
     * *after* the peer has stopped. This implementation does nothing because a publisher doesn't
     * request data.
     *
     * @param[in] peer  Peer with unsatisfied requests.
     */
    virtual void reassignRequests(Peer::Pimpl peer) {
    }

    /**
     * Executes a peer. Meant to be the start routine of a separate thread.
     * @param[in] peer  The peer to execute
     */
    void runPeer(Peer::Pimpl peer) {
        try {
            peer->run();
            LOG_INFO("Peer %s terminated", peer->to_string().data());
        } // Peer was added to active peer set
        catch (const std::exception& ex) {
            LOG_ERROR(ex);
            LOG_ERROR("Peer %s failed", peer->to_string().data());
        }

        // Must occur before removePeer() because missed() requires that `peerMap` contain `peer`
        reassignRequests(peer);
        removePeer(peer);
        peerRemoved(peer);
    }

    /**
     * Adds a peer. Adds the peer to the set of active peers and the bookkeeper; then starts the
     * peer. Notifies waiting threads about the change to the set of active peers.
     *
     * @pre                   Mutex is unlocked
     * @param[in] peer        Peer to be added
     * @retval    true        Peer added
     * @retval    false       Peer was previously added
     * @post                  Mutex is unlocked
     */
    bool add(Peer::Pimpl peer) {
        if (!addPeer(peer))
            return false;
        Thread(&P2pMgrImpl::runPeer, this, peer).detach();
        return true;
    }

    /**
     * Returns the server-side peer corresponding to an accepted connection from a client-side peer.
     * The returned peer is connected to its remote counterpart but not yet enabled/active.
     *
     * @return  Server-side peer. Will test false if the P2P-server has been halted.
     */
    virtual Peer::Pimpl accept() =0;

    /**
     * Runs the P2P-server. Accepts connections from remote peers and adds them to the set of active
     * server-side peers. Intended to be a start function for a separate thread.
     *
     * NB: `noexcept` is incompatible with exception-based thread cancellation.
     */
    void acceptPeers() {
        try {
            auto doSomething = [&]{return numSrvrPeers < maxSrvrPeers || state == State::STOPPING;};
            for (;;) {
                {
                    Lock lock{stateMutex};
                    stateCond.wait(lock, doSomething);
                    if (state == State::STOPPING)
                        break;
                    /*
                     * The mutex is released immediately because  a server-side peer is only added
                     * by the current thread and the mutex is locked on other threads that access
                     * the peer-set.
                     */
                }

                try {
                    auto peer = accept(); // Construction causes exchange of P2P-server information

                    if (!peer)
                        break; // `p2pSrvr->halt()` was called

                    // The tracker now contains information on the remote peer's P2P server

                    if (!add(peer)) { // Locks the peer-set mutex
                        LOG_WARNING("Peer %s was previously accepted", peer->to_string().data());
                    }
                    else {
                        LOG_NOTE("Accepted connection from peer %s", peer->to_string().data());
                        updateTier(peer->getTier());
                    }
                }
                catch (const InvalidArgument& ex) {
                    // The remote peer sent bad information
                    LOG_WARNING(ex);
                    continue;
                }
            }
        }
        catch (const std::exception& ex) {
            try {
                std::throw_with_nested(RUNTIME_ERROR("P2pSrvr::accept() failure"));
            }
            catch (const std::exception& ex) {
                setException(ex);
            }
        }
    }

    /**
     * Improves the set of peers by periodically removing the worst-performing peer. The
     * peer-adding threads will be notified and a replacement peer will be added. Intended to be the
     * start function for a separate thread.
     *
     * @param[in] numPeers    Number of relevant peers
     * @param[in] maxPeers    Maximum number of relevant peers
     */
    void improvePeers(
            int&      numPeers,
            const int maxPeers) {
        try {
            Lock lock{stateMutex};
            auto pred = [&]{return peersChanged || state == State::STOPPING;};

            for (;;) {
                bookkeeper->reset();
                peersChanged = false;

                // Wait until the set of relevant peers is unchanged for the evaluation duration
                if (stateCond.wait_for(lock, evalTime, pred)) {
                    // Either the set of active peers changed or this instance is stopping
                    if (state == State::STOPPING)
                        break;
                }
                else {
                    // The set of active peers has been unchanged for the evaluation duration
                    if (numPeers >= maxPeers) {
                        auto worstPeer = bookkeeper->getWorstPeer();
                        if (worstPeer) {
                            LOG_DEBUG("Halting peer %s", worstPeer->to_string().data());
                            worstPeer->halt(); // `runPeer()` will remove peer and notify observers
                        }
                    }
                }
            }
        }
        catch (const std::exception& ex) {
            setException(ex);
        }
    }

    /**
     * Cancels a thread and joins it.
     * @param[in] thread  The thread to cancel
     */
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
     * @retval    true       Peer should be notified
     * @retval    false      Peer should not be notified
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
     * @retval    true       Peer should be notified
     * @retval    false      Peer should not be notified
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
     * @param[in,out] tracker       ///< Tracks available but unused P2P-servers
     * @param[in]     maxPeers      ///< Maximum number of peers
     * @param[in]     maxSrvrPeers  ///< Maximum number of server-side-constructed peers
     * @param[in]     evalTime      ///< Peer evaluation time in seconds
     */
    P2pMgrImpl(
            Node&          node,
            Tracker        tracker,
            const int      maxPeers,
            const int      maxSrvrPeers,
            const int      evalTime)
        : state(State::INIT)
        , stateMutex()
        , node(node)
        , peersChanged(false)
        , stateCond()
        , stopSem()
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
        , srvrInfo()
    {
        if (maxSrvrPeers <= 0)
            throw INVALID_ARGUMENT("Invalid maximum number of server-side peers: " +
                    std::to_string(maxSrvrPeers));

        if (::sem_init(&stopSem, 0, 0) == -1)
            throw SYSTEM_ERROR("Couldn't initialize semaphore");
    }

    virtual ~P2pMgrImpl() noexcept {
        Guard guard{stateMutex};
        LOG_ASSERT(state == State::INIT || state == State::STOPPED);
        ::sem_destroy(&stopSem);
    }

    Tracker& getTracker() override {
        return tracker;
    }

    P2pSrvrInfo getSrvrInfo() override {
        return updateSrvrInfo();
    }

    virtual void saveRmtSrvrInfo(const P2pSrvrInfo& srvrInfo) =0;

    void run() override {
        startImpl();
        ::sem_wait(&stopSem); // Blocks until `halt()` called or `threadEx` is true
        stopImpl();
        threadEx.throwIfSet();
    }

    void halt() override {
        int semval = 0;
        ::sem_getvalue(&stopSem, &semval);
        if (semval < 1)
            ::sem_post(&stopSem);
    }

    void waitForSrvrPeer() override {
        Lock lock{stateMutex};
        stateCond.wait(lock, [&]{return numSrvrPeers > 0 || state == State::STOPPING;});
    }

    void notify(const ProdId prodId) override {
        LOG_DEBUG("Notifying peers about product %s", prodId.to_string().data());
        notify<ProdId>(prodId);
    }

    void notify(const DataSegId segId) override {
        LOG_DEBUG("Notifying peers about data-segment %s", segId.to_string().data());
        notify<DataSegId>(segId);
    }

    ProdIdSet subtract(ProdIdSet rhs) const override {
        return node.subtract(rhs);
    }

    ProdIdSet getProdIds() const override {
        return node.getProdIds();
    }

    /**
     * Returns information on a product.
     * @param[in] prodId  Product identifier
     * @return            Information on the given product
     */
    ProdInfo getDatum(const ProdId prodId) {
        return node.recvRequest(prodId);
    }

    /**
     * Returns a data segment.
     * @param[in] segId  The data segment identifier
     * @return           The corresponding data segment. Will be invalid if it doesn't exist.
     */
    DataSeg getDatum(const DataSegId segId) {
        return node.recvRequest(segId);
    }

    void recvAdd(Tracker tracker) override {
        tracker.erase(srvrInfo.srvrAddr); // Connecting to oneself is useless
        this->tracker.insert(tracker);
    }
};

/**************************************************************************************************/

/// Publisher's P2P manager implementation
class PubP2pMgrImpl final : public P2pMgrImpl
{
    PubP2pSrvr::Pimpl p2pSrvr;   ///< P2p-server

protected:
    Peer::Pimpl accept() override {
        return p2pSrvr->accept(*this);
    }

    void startThreads() override {
        LOG_ASSERT(!stateMutex.try_lock());
        LOG_ASSERT(state == State::INIT);

        acceptThread  = Thread(&PubP2pMgrImpl::acceptPeers, this);
        improveThread = Thread(&PubP2pMgrImpl::improvePeers, this, std::ref(numSrvrPeers),
                maxSrvrPeers);
    }

    void stopThreads() override {
        {
            Guard guard{stateMutex};

            p2pSrvr->halt();        // Causes `accept()` to return a false peer

            state = State::STOPPING;
            stateCond.notify_all();
        }

        if (improveThread.joinable())
            improveThread.join();
        if (acceptThread.joinable())
            acceptThread.join();
    }

    P2pSrvrInfo& updateSrvrInfo() override {
        Guard guard{stateMutex};

        srvrInfo.numAvail = maxSrvrPeers - numSrvrPeers;
        srvrInfo.valid = SysClock::now();

        return srvrInfo;
    }

    void updateTier(const P2pSrvrInfo::Tier tier) override {
        // Does nothing because the number of hops to the publisher is always zero
    }

    void incPeerCount(Peer::Pimpl peer) override {
        LOG_ASSERT(!stateMutex.try_lock());
        ++numSrvrPeers;
    }

    void decPeerCount(Peer::Pimpl peer) override {
        LOG_ASSERT(!stateMutex.try_lock());
        --numSrvrPeers;
    }

    bool shouldNotify(
            Peer::Pimpl peer,
            ProdId      prodId) override {
        LOG_ASSERT(!stateMutex.try_lock());
        // Publisher's peer should always notify the remote peer
        return true;
    }

    bool shouldNotify(
            Peer::Pimpl      peer,
            DataSegId dataSegId) override {
        LOG_ASSERT(!stateMutex.try_lock());
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
     * @param[in] p2pSrvr       P2P-server
     * @param[in] maxPeers      Maximum number of peers
     * @param[in] evalTime      Evaluation time for poorest-performing peer in seconds
     */
    PubP2pMgrImpl(PubNode&                 pubNode,
                  const PubP2pSrvr::Pimpl  p2pSrvr,
                  const int                maxPeers,
                  const int                evalTime)
        : P2pMgrImpl(pubNode, Tracker{}, maxPeers, maxPeers, evalTime)
        , p2pSrvr(p2pSrvr)
    {
        bookkeeper = Bookkeeper::createPub(maxPeers);
        srvrInfo = P2pSrvrInfo{p2pSrvr->getSrvrAddr(), maxPeers, 0};
    }

    /**
     * Copy constructs.
     * @param[in] impl  Pointer to an implementation
     */
    PubP2pMgrImpl(const PubP2pMgrImpl& impl) =delete;

    /**
     * Copy assigns.
     * @param[in] rhs  The other instance
     * @return         A reference to this just-assigned instance
     */
    PubP2pMgrImpl& operator=(const PubP2pMgrImpl& rhs) =delete;

    void saveRmtSrvrInfo(const P2pSrvrInfo& srvrInfo) override {
        tracker.insert(srvrInfo);
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
            Guard guard{stateMutex};
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
            Guard guard{stateMutex};
            if (peerMap.count(rmtAddr) == 0)
                return DataSeg{};
            bookkeeper->requested(peerMap.at(rmtAddr));
        }
        return getDatum(segId);
    }

    void peerRemoved(Peer::Pimpl peer) override {
        LOG_ASSERT(srvrInfo.numAvail > 0);
        --srvrInfo.numAvail;
    }
};

P2pMgr::Pimpl P2pMgr::create(
        PubNode&       pubNode,
        const SockAddr p2pSrvrAddr,
        const unsigned maxPeers,
        const unsigned maxPendConn,
        const unsigned evalTime) {
    auto p2pSrvr = PubP2pSrvr::create(p2pSrvrAddr, maxPendConn);
    return Pimpl{new PubP2pMgrImpl(pubNode, p2pSrvr, maxPeers, evalTime)};
}

/**************************************************************************************************/

/// Subscribing P2P manager implementation
class SubP2pMgrImpl final :  public P2pMgrImpl, public SubP2pMgr
{
    SubNode&           subNode;       ///< Subscriber's node
    const int          maxClntPeers;  ///< Maximum number of client-side peers
    int                numClntPeers;  ///< Number of client-side peers
    int                timeout;       ///< Timeout, in ms, for connecting to remote P2P-server
    SubP2pSrvr::Pimpl  p2pSrvr;       ///< Subscriber's P2P-server
    Thread             connectThread; ///< Thread for creating client-side peers

    /**
     * Creates client-side peers. Meant to be the start routine of a separate thread.
     * NB: `noexcept` is incompatible with thread cancellation.
     */
    void connectPeers() {
        try {
            auto pred = [&]{return numClntPeers < maxClntPeers || state == State::STOPPING;};
            for (;;) {
                {
                    LOG_TRACE("Locking the peer-set mutex");
                    Lock lock{stateMutex};
                    LOG_TRACE("Waiting for the need for a peer or stopping");
                    stateCond.wait(lock, pred);

                    if (state == State::STOPPING)
                        break;
                    /*
                     * The peer-set mutex is released immediately because a client-side peer is only
                     * added by the current thread.
                     */
                }

                LOG_TRACE("Removing tracker head");
                auto rmtSrvrAddr = tracker.getNextAddr(); // Blocks if empty. Cancellation point.
                if (!rmtSrvrAddr) {
                    // tracker.halt() called
                    LOG_ASSERT(state == State::STOPPING);
                    break;
                }

                try {
                    LOG_TRACE("Creating peer");
                    /*
                     * P2P-server information is exchanged during peer construction but
                     * `saveRmtSrvrInfo()` is not called.
                     */
                    auto peer = Peer::create(*this, rmtSrvrAddr);

                    /*
                     * Consequently, information on the remote peer's P2P server is explicitly
                     * saved. This procedure prevents this instance's tier number from being
                     * modified by a peer whose `run()` function won't be called while still
                     * allowing the information on the remote P2P-server to be saved.
                     */
                    auto rmtSrvrInfo = peer->getRmtSrvrInfo(); // NB: A copy is made
                    tracker.insert(rmtSrvrInfo);

                    /*
                     * To prevent an orphan network, use only a client-side peer whose remote peer
                     * provides a path to the publisher and can accept a new client.
                     */
                    if (!rmtSrvrInfo.validTier()) {
                        LOG_DEBUG("Remote P2P-server " + rmtSrvrInfo.srvrAddr.to_string() +
                                " has no path to publisher");
                    }
                    else if (rmtSrvrInfo.numAvail == 0) {
                        LOG_DEBUG("Remote P2P-server " + rmtSrvrInfo.srvrAddr.to_string() +
                                " can't accept a new client");
                    }
                    else {
                        try {
                            LOG_TRACE("Adding peer to peer-set");
                            if (!add(peer)) {
                                LOG_DEBUG("Throwing logic error");
                                throw LOGIC_ERROR("Already connected to " + rmtSrvrAddr.to_string());
                            }

                            try {
                                LOG_NOTE("Connected to %s", rmtSrvrAddr.to_string().data());

                                updateTier(peer->getTier()); // Now the tier number can be modified
                            } // Peer is in peer-set and active
                            catch (...) {
                                LOG_DEBUG("Halting peer");
                                peer->halt(); // runPeer() removes peer and notifies observers
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
                                // Peer is unavailable
                                LOG_NOTE(ex);
                                tracker.offline(rmtSrvrAddr);
                            }
                            else {
                                LOG_DEBUG("Throwing exception %s", ex.what());
                                throw;
                            }
                        }
                    } // Remote P2P-server is available
                } // `rmtSrvrAddr` obtained from tracker
                catch (const InvalidArgument& ex) {
                    // The remote peer sent bad information
                    LOG_WARNING(ex);
                    tracker.offline(rmtSrvrAddr);
                }
                catch (const std::exception& ex) {
                    // The exception is unrelated to peer availability
                    tracker.disconnected(rmtSrvrAddr); // Makes it available after a delay
                }
            } // New, client-side peer loop
        }
        catch (const std::exception& ex) {
            LOG_DEBUG("Setting exception %s", ex.what());
            setException(ex);
        }
        catch (...) {
            LOG_DEBUG("Caught cancellation exception?");
            throw; // A thread cancellation "exception" *must* be rethrown
        }
    }

    /**
     * Reassigns a request that couldn't be satisfied by a peer to another peer, if possible.
     *
     * @tparam ID   Type of requested object. Either `ProdId` or `DataSegId`.
     * @param peer  Peer that couldn't satisfy the request
     * @param id    Requested object
     */
    template<class ID>
    void reassign(
            const Peer::Pimpl peer,
            const ID&         id) {
        auto altPeer = bookkeeper->getAltPeer(peer, id);
        if (altPeer)
            altPeer->request(id);
    }

protected:
    Peer::Pimpl accept() override {
        return p2pSrvr->accept(*this);
    }

    void startThreads() override {
        LOG_ASSERT(!stateMutex.try_lock());
        LOG_ASSERT(state == State::INIT);

        connectThread = Thread(&SubP2pMgrImpl::connectPeers, this);
        acceptThread  = Thread(&SubP2pMgrImpl::acceptPeers, this);
        improveThread = Thread(&SubP2pMgrImpl::improvePeers, this, std::ref(numClntPeers),
                maxClntPeers);
    }

    void stopThreads() override {
        {
            Guard guard{stateMutex};

            p2pSrvr->halt();        // Causes `accept()` to return a false peer
            tracker.halt();         // Causes `tracker.getNext()` to return a false object

            state = State::STOPPING;
            stateCond.notify_all();
        }

        if (improveThread.joinable())
            improveThread.join();
        if (acceptThread.joinable())
            acceptThread.join();
        if (connectThread.joinable())
            connectThread.join();
    }

    P2pSrvrInfo& updateSrvrInfo() override {
        Guard guard{stateMutex};

        srvrInfo.numAvail = maxSrvrPeers - numSrvrPeers;
        srvrInfo.valid = SysClock::now();

        return srvrInfo;
    }

    /**
     * Updates the tier number of the local P2P-server if appropriate.
     * @param[in] tier  A candidate tier number for the local P2P-server
     */
    void updateTier(const P2pSrvrInfo::Tier tier) override {
        Guard guard{stateMutex};
        // A subscriber's P2P-server can't be tier 0
        if (tier > 0 && (!srvrInfo.validTier() || tier < srvrInfo.tier))
            srvrInfo.tier = tier;
    }

    void incPeerCount(Peer::Pimpl peer) override {
        LOG_ASSERT(!stateMutex.try_lock());
        peer->isClient() ? ++numClntPeers : ++numSrvrPeers;
    }

    void decPeerCount(Peer::Pimpl peer) override {
        LOG_ASSERT(!stateMutex.try_lock());
        peer->isClient() ? --numClntPeers : --numSrvrPeers;
    }

    /**
     * Handles a peer being removed from the set of active peers. If the peer was constructed
     * client-side, then the address of the remote P2P-server is added to the delay queue so that it
     * might be tried again later.
     * @param[in] peer  The peer that was removed from the set of active peers
     */
    void peerRemoved(Peer::Pimpl peer) override {
        // Remote socket address is remote P2P-server's only for a peer constructed client-side
        if (peer->isClient())
            tracker.disconnected(peer->getRmtAddr());
    }

    /**
     * Reassigns unsatisfied requests for data from a peer to other peers. Should be executed only
     * *after* the peer has stopped.
     *
     * @param[in] peer  Peer with unsatisfied requests.
     */
    void reassignRequests(Peer::Pimpl peer) override {
        peer->drainPending(); // Calls `missed()` with unsatisfied requests
    }

    bool shouldNotify(
            Peer::Pimpl  peer,
            const ProdId prodId) override {
        LOG_ASSERT(!stateMutex.try_lock());
        // Remote peer is subscriber & doesn't have the datum => notify
        return !peer->isRmtPub() && bookkeeper->shouldNotify(peer, prodId);
    }

    bool shouldNotify(
            Peer::Pimpl     peer,
            const DataSegId dataSegId) override {
        LOG_ASSERT(!stateMutex.try_lock());
        // Remote peer is subscriber & doesn't have the datum => notify
        return !peer->isRmtPub() && bookkeeper->shouldNotify(peer, dataSegId);
    }

public:
    /**
     * Creates an implementation of a subscribing P2P manager. Creates a P2P-server listening on a
     * socket but doesn't do anything with it until `run()` is called.
     *
     * @param[in] subNode       Subscriber's node
     * @param[in] tracker       Pool of addresses of P2P-servers
     * @param[in] peerConnSrvr  Peer-connection server
     * @param[in] timeout       Timeout, in ms, for connecting to remote P2P-servers. -1 => default
     *                          timeout; 0 => immediate return.
     * @param[in] maxPeers      Maximum number of peers. Might be adjusted upwards.
     * @param[in] evalTime      Evaluation interval for poorest-performing peer in seconds
     * @return                  Subscribing P2P manager
     * @see `getPeerSrvrAddr()`
     */
    SubP2pMgrImpl(SubNode&            subNode,
                  Tracker             tracker,
                  PeerConnSrvr::Pimpl peerConnSrvr,
                  const int           timeout,
                  const int           maxPeers,
                  const int           evalTime)
        : P2pMgrImpl(subNode, tracker, ((maxPeers+1)/2)*2, (maxPeers+1)/2, evalTime)
        , subNode(subNode)
        , maxClntPeers(maxSrvrPeers)
        , numClntPeers(0)
        , timeout(timeout)
        , p2pSrvr(SubP2pSrvr::create(peerConnSrvr))
        , connectThread()
    {
        bookkeeper = Bookkeeper::createSub(maxPeers);
        srvrInfo = P2pSrvrInfo{p2pSrvr->getSrvrAddr(), maxSrvrPeers};
        // Ensure that this instance doesn't try to connect to itself
        tracker.erase(p2pSrvr->getSrvrAddr());
    }

    /// Copy constructs
    SubP2pMgrImpl(const SubP2pMgrImpl& impl) =delete;

    /// Copy assignment operator
    SubP2pMgrImpl& operator=(const SubP2pMgrImpl& rhs) =delete;

    Tracker& getTracker() {
        return P2pMgrImpl::getTracker();
    }

    P2pSrvrInfo getSrvrInfo() {
        return P2pMgrImpl::getSrvrInfo();
    }

    void run() {
        P2pMgrImpl::run();
    }

    void halt() {
        P2pMgrImpl::halt();
    }

    void saveRmtSrvrInfo(const P2pSrvrInfo& srvrInfo) override {
        tracker.insert(srvrInfo);
        updateTier(srvrInfo.getClntTier());
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

    ProdIdSet subtract(ProdIdSet other) const override {
        return P2pMgrImpl::subtract(other);
    }

    ProdIdSet getProdIds() const override {
        return P2pMgrImpl::getProdIds();
    }

    bool recvNotice(const ProdId   prodId,
                    const SockAddr rmtAddr) {
        // Must not exist and not been previously requested
        if (subNode.shouldRequest(prodId)) {
            Guard guard{stateMutex};
            if (peerMap.count(rmtAddr))
                return bookkeeper->shouldRequest(peerMap[rmtAddr], prodId);
        }
        return false;
    }
    bool recvNotice(const DataSegId segId,
                    SockAddr        rmtAddr) {
        // Must not exist and not been previously requested
        if (subNode.shouldRequest(segId)) {
            Guard guard{stateMutex};
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
        Guard guard{stateMutex};
        if (state != State::STOPPING) {
            if (peerMap.count(rmtAddr)) {
                auto& peer = peerMap[rmtAddr];
                reassign(peerMap[rmtAddr], prodId);
            }
        }
    }
    void missed(const DataSegId dataSegId,
                SockAddr        rmtAddr) {
        Guard guard{stateMutex};
        if (state != State::STOPPING) {
            if (peerMap.count(rmtAddr)) {
                auto& peer = peerMap[rmtAddr];
                reassign(peerMap[rmtAddr], dataSegId);
            }
        }
    }

    /**
     * Receives product information from a remote peer.
     *
     * @param[in] datum     Product information
     * @param[in] rmtAddr   Socket address of remote peer
     */
    template<class DATUM>
    void recvData(const DATUM datum,
                  SockAddr    rmtAddr) {
        Guard      guard{stateMutex};

        if (state != State::STOPPING) {
            const auto id = datum.getId();

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
                if (bookkeeper->received(peer, id)) {
                    subNode.recvP2pData(datum);

                    LOG_DEBUG("Notifying peers about datum %s", id.to_string().data());
                    for (auto p : peerSet) {
                        if (shouldNotify(p, datum.getId()))
                            p->notify(id);
                    }
                    LOG_TRACE("Erasing bookkeeper entry");
                    bookkeeper->erase(id); // No longer relevant
                }
                else {
                    LOG_WARNING("Datum " + id.to_string() + " was unexpected");
                }
            }
            else {
                LOG_DEBUG("Remote P2P-server " + rmtAddr.to_string() + " not found");
            }
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

SubP2pMgr::Pimpl SubP2pMgr::create(
        SubNode&                  subNode,
        Tracker                   tracker,
        const PeerConnSrvr::Pimpl peerConnSrvr,
        const int                 timeout,
        const int                 maxPeers,
        const int                 evalTime) {
    return Pimpl{new SubP2pMgrImpl{subNode, tracker, peerConnSrvr, timeout, maxPeers,
            evalTime}};
}

SubP2pMgr::Pimpl SubP2pMgr::create(
        SubNode&          subNode,
        Tracker           tracker,
        const SockAddr    p2pSrvrAddr,
        const int         maxPendConn,
        const int         timeout,
        const int         maxPeers,
        const int         evalTime) {
    auto peerConnSrvr = PeerConnSrvr::create(p2pSrvrAddr, maxPendConn);
    return create(subNode, tracker, peerConnSrvr, timeout, maxPeers, evalTime);
}

} // namespace
