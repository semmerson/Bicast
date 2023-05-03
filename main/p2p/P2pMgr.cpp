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
        Guard guard{peerSetMutex};
        for (auto iter = peerSet.begin(), stop = peerSet.end(); iter != stop; ) {
            auto peer = *(iter++); // Increment prevents becoming invalid
            if (shouldNotify(peer, id))
                peer->notify(id);
        }
    }

    bool addPeer(Peer::Pimpl peer) {
        Guard guard{peerSetMutex};

        if (!peerSet.insert(peer).second)
            return false;

        peerMap[peer->getRmtAddr()] = peer;
        const auto added = bookkeeper->add(peer);
        LOG_ASSERT(added);
        incPeerCount(peer);
        peersChanged = true;
        peerSetCond.notify_all();

        return true;
    }

    void removePeer(Peer::Pimpl peer) {
        Guard guard{peerSetMutex};

        if (peerSet.erase(peer)) {
            const auto n = peerMap.erase(peer->getRmtAddr());
            LOG_ASSERT(n);
            const auto existed = bookkeeper->erase(peer);
            LOG_ASSERT(existed);
            decPeerCount(peer);
            peersChanged = true;
            peerSetCond.notify_all();
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
    mutable Mutex        peerSetMutex;  ///< Guards set of active peers
    mutable Cond         peerSetCond;   ///< For changing set of active peers
    mutable sem_t        stopSem;       ///< For async-signal-safe stopping
    Tracker              tracker;       ///< Socket addresses of available but unused P2P-servers
    ThreadEx             threadEx;      ///< Exception thrown by internal threads
    const unsigned       maxSrvrPeers;  ///< Maximum number of server-side peers
    unsigned             numSrvrPeers;  ///< Number of server-side peers
    PeerSet              peerSet;       ///< Set of active peers
    PeerMap              peerMap;       ///< Map from remote address to peer
    Bookkeeper::Pimpl    bookkeeper;    ///< Monitors peer performance
    Thread               acceptThread;  ///< Creates server-side peers
    Thread               improveThread; ///< Improves the set of peers
    std::chrono::seconds evalTime;      ///< Evaluation interval for poorest-performing peer
    P2pSrvrInfo         peerSrvrInfo;  ///< Information on the P2P-server

    /**
     * @pre               The state mutex is unlocked
     * @throw LogicError  The state isn't INIT
     * @post              The state is STARTED
     * @post              The state mutex is unlocked
     */
    void startImpl() {
        Guard guard{stateMutex};
        if (state != State::INIT)
            throw LOGIC_ERROR("Instance can't be re-executed");
        startThreads();
        state = State::STARTED;
    }

    /**
     * Idempotent.
     *
     * @pre               The state mutex is unlocked
     * @throw LogicError  The state is stopping
     * @post              The state is STOPPED
     * @post              The state mutex is unlocked
     */
    void stopImpl() {
        Lock lock{stateMutex};
        LOG_ASSERT(state == State::STARTED);

        state = State::STOPPING;

        /*
         * Ensure that the set of active peers will no longer change so that it can be iterated
         * through without locking it, which can deadlock with `SubP2pMgr::recvData()`.
         */
        stopThreads();

        for (auto peer : peerSet) {
            peer->halt();
        }
        peerSetCond.wait(lock, [&]{return peerSet.empty();});

        state = State::STOPPED;
    }

    /**
     * Sets the exception to be thrown.
     * @param[in] ex  The exception to be thrown
     */
    void setException(const std::exception& ex) {
        threadEx.set(ex);
        ::sem_post(&stopSem);
    }

    /**
     * Handles a peer being removed from the set of active peers. Does nothing by default because a
     * publisher's P2P manager doesn't have client-side peers -- so there's nothing to be done.
     *
     * @param[in] peer  The peer that was removed from the set of active peers
     */
    virtual void peerRemoved(Peer::Pimpl peer) {
    }

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
     * Handles a peer being added to the set of active peers. Sends to the remote peer
     *   - An indicator of whether or not the local P2P node is a path to the publisher; and
     *   - The addresses of known, potential P2P-servers.
     *
     * @param[in] peer     Peer whose remote counterpart is to be notified.
     */
    virtual void peerAdded(Peer::Pimpl peer) =0;

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
        peerAdded(peer);
        return true;
    }

    /**
     * Returns the server-side peer corresponding to an accepted connection from a client-side peer.
     * The returned peer is connected to its remote counterpart but not yet receiving from it.
     *
     * @return  Server-side peer. Will test false if the P2P-server has been halted.
     */
    virtual Peer::Pimpl accept() =0;

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
            const unsigned maxPeers,
            const unsigned maxSrvrPeers,
            const unsigned evalTime)
        : state(State::INIT)
        , stateMutex()
        , node(node)
        , peersChanged(false)
        , peerSetMutex()
        , peerSetCond()
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
        , peerSrvrInfo()
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

    void run() override {
        startImpl();
        ::sem_wait(&stopSem); // Blocks until `halt()` called or `threadEx` is true
        //stateCond.wait(lock, [&]{return state == State::STOPPING;});
        stopImpl();
        threadEx.throwIfSet();
    }

    void halt() override {
        int semval = 0;
        ::sem_getvalue(&stopSem, &semval);
        if (semval < 1)
            ::sem_post(&stopSem);
    }

    /**
     * Runs the P2P-server. Accepts connections from remote peers and adds them to the set of active
     * server-side peers. Must be public because it's the start function for a thread.
     *
     * NB: `noexcept` is incompatible with thread cancellation.
     */
    void acceptPeers() {
        try {
            auto doSomething = [&]{return numSrvrPeers < maxSrvrPeers || state == State::STOPPING;};
            for (;;) {
                {
                    Lock lock{peerSetMutex};
                    peerSetCond.wait(lock, doSomething);
                    if (state == State::STOPPING)
                        break;
                    /*
                     * The mutex is released immediately because  a server-side peer is only added
                     * by the current thread.
                     */
                }

                auto peer = accept();
                if (!peer)
                    break; // `peerSrvr->halt()` was called

                if (!add(peer)) {
                    LOG_WARNING("Peer %s was previously accepted", peer->to_string().data());
                }
                else {
                    LOG_NOTE("Accepted connection from peer %s", peer->to_string().data());
                }
            }
        }
        catch (const std::exception& ex) {
            try {
                std::throw_with_nested(RUNTIME_ERROR("PeerSrvr::accept() failure"));
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
            auto doSomething = [&]{return peersChanged || state == State::STOPPING;};

            for (;;) {
                bookkeeper->reset();
                peersChanged = false;

                // Wait until the set of relevant peers is unchanged for the evaluation duration
                if (peerSetCond.wait_for(lock, evalTime, doSomething)) {
                    if (state == State::STOPPING)
                        break;
                }
                else if (numPeers >= maxPeers) {
                    auto worstPeer = bookkeeper->getWorstPeer();
                    if (worstPeer) {
                        LOG_DEBUG("Halting peer %s", worstPeer->to_string().data());
                        worstPeer->halt(); // Causes runPeer() to remove peer and notify observers
                    }
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

    /**
     * Receives from a remote peer, an indication of whether or not it's a path to the publisher.
     * @param[in] rmtIsPubPath  Is the remote peer a path to the publisher?
     * @param[in] rmtAddr       Socket address of remote peer
     */
    void recvHavePubPath(
            const bool     rmtIsPubPath,
            const SockAddr rmtAddr) override {
        /*
         * To prevent an orphan network, terminate client-side peers whose remote counterpart
         * doesn't provide a path to the publisher.
         */
        if (!rmtIsPubPath) {
            Guard guard{peerSetMutex};
            if (peerMap.count(rmtAddr)) {
                auto& peer = peerMap[rmtAddr];
                if (peer->isClient()) {
                    LOG_DEBUG("Disconnecting from peer " + peer->getRmtAddr().to_string() +
                            " because it isn't a path to the publisher");
                    peer->halt(); // Causes runPeer() to remove peer and notify observers
                }
            }
        }
    }

    void recvAdd(const P2pSrvrInfo& info) override {
        if (info.srvrAddr != peerSrvrInfo.srvrAddr) // Connecting to oneself is useless
            this->tracker.insert(info);
    }

    void recvAdd(Tracker tracker) override {
        tracker.erase(peerSrvrInfo.srvrAddr); // Connecting to oneself is useless
        this->tracker.insert(tracker);
    }

    void recvRemove(const SockAddr peerSrvrAddr) override {
        this->tracker.erase(peerSrvrAddr);
    }
    void recvRemove(const Tracker tracker) override {
        this->tracker.erase(tracker);
    }
};

/**************************************************************************************************/

/// Publisher's P2P manager implementation
class PubP2pMgrImpl final : public P2pMgrImpl
{
    PubPeerSrvr::Pimpl peerSrvr;   ///< Peer-server

protected:
    Peer::Pimpl accept() override {
        return peerSrvr->accept(*this);
    }

    void startThreads() override {
        LOG_ASSERT(state == State::INIT);

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
        LOG_ASSERT(!stateMutex.try_lock());
        LOG_ASSERT(state == State::STOPPING);

        peerSrvr->halt(); // Causes `accept()` to return a false peer
        {
            Guard guard{peerSetMutex};
            peerSetCond.notify_all(); // Signals improvement and accept threads to terminate
        } // `peerSetMutex` must be unlocked for the threads to terminate

        improveThread.join();
        acceptThread.join();
    }

    void peerAdded(Peer::Pimpl peer) override {
        /*
         * The remote peer knows that this instance is a publisher; therefore, there's no need to
         * inform it that this instance is a path to the publisher.
         *
         * `getPeerSetTracker()` is useless because all those peers were constructed server-side,
         * which means that their remote addresses aren't those of their P2P-servers'.
         */
        peer->add(tracker);
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
     * @param[in] peerSrvr      P2P-server
     * @param[in] maxPeers      Maximum number of peers
     * @param[in] evalTime      Evaluation time for poorest-performing peer in seconds
     */
    PubP2pMgrImpl(PubNode&                 pubNode,
                  const PubPeerSrvr::Pimpl peerSrvr,
                  const unsigned           maxPeers,
                  const unsigned           evalTime)
        : P2pMgrImpl(pubNode, Tracker{}, maxPeers, maxPeers, evalTime)
        , peerSrvr(peerSrvr)
    {
        bookkeeper = Bookkeeper::createPub(maxPeers);
        peerSrvrInfo = P2pSrvrInfo{peerSrvr->getSrvrAddr(), 0, maxPeers, SysClock::now()};
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

    P2pSrvrInfo getSrvrInfo() override {
        Guard guard{peerSetMutex};
        peerSrvrInfo.numAvail = maxSrvrPeers - numSrvrPeers;
        peerSrvrInfo.valid = SysClock::now();
        return peerSrvrInfo;
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
        const unsigned maxPendConn,
        const unsigned evalTime) {
    auto peerSrvr = PubPeerSrvr::create(peerSrvrAddr, maxPendConn);
    return Pimpl{new PubP2pMgrImpl(pubNode, peerSrvr, maxPeers, evalTime)};
}

/**************************************************************************************************/

/// Subscribing P2P manager implementation
class SubP2pMgrImpl final : public SubP2pMgr, public P2pMgrImpl
{
    SubNode&            subNode;       ///< Subscriber's node
    const unsigned      maxClntPeers;  ///< Maximum number of client-side peers
    unsigned            numClntPeers;  ///< Number of client-side peers
    int                 timeout;       ///< Timeout, in ms, for connecting to remote P2P-server
    SubPeerSrvr::Pimpl  peerSrvr;      ///< Subscriber's P2P-server
    Thread              connectThread; ///< Thread for creating client-side peers
    P2pSrvrInfo::Tier  tier;          ///< Number of hops to the publisher

    /**
     * Creates client-side peers. Meant to be the start routine of a separate thread.
     * NB: `noexcept` is incompatible with thread cancellation.
     */
    void connectPeers() {
        try {
            auto needPeer = [&]{return numClntPeers < maxClntPeers || state == State::STOPPING;};
            for (;;) {
                {
                    LOG_TRACE("Locking the peer-set mutex");
                    Lock lock{peerSetMutex};
                    LOG_TRACE("Waiting for the need for a peer");
                    peerSetCond.wait(lock, needPeer);

                    if (state == State::STOPPING)
                        break;
                    /*
                     * The peer-set mutex is released immediately because a client-side peer is only
                     * added by the current thread.
                     */
                }

                LOG_TRACE("Removing tracker head");
                auto rmtSrvrAddr = tracker.getNext(); // Blocks if empty. Cancellation point.
                if (!rmtSrvrAddr) {
                    // tracker.halt() called
                    LOG_ASSERT(state == State::STOPPING);
                    break;
                }

                try {
                    LOG_TRACE("Creating peer");
                    auto peer = Peer::create(*this, rmtSrvrAddr);
                    try {
                        LOG_TRACE("Adding peer to peer-set");
                        if (!add(peer)) {
                            LOG_DEBUG("Throwing logic error");
                            throw LOGIC_ERROR("Already connected to " + rmtSrvrAddr.to_string());
                        }

                        try {
                            LOG_NOTE("Connected to %s", rmtSrvrAddr.to_string().data());
                        } // Peer is active and in peer-set
                        catch (...) {
                            LOG_DEBUG("Halting peer");
                            peer->halt(); // Causes runPeer() to remove peer and notify observers
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
                } // `rmtSrvrAddr` removed from tracker
                catch (const std::exception& ex) {
                    // The exception is unrelated to peer availability
                    tracker.disconnected(rmtSrvrAddr); // Put it back
                }
            } // New client-side peer loop
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

    Tracker getPeerSetTracker() {
        Guard   guard{peerSetMutex};
        Tracker tracker{peerSet.size()};
        for (auto peer : peerSet) {
            /*
             * A server-side peer's remote socket address isn't the remote P2P-server's address.
             * Getting that address is handled by client-side peer calling `add(peerSrvrAddr)`.
             */
            if (peer->isClient()) {
                LOG_ASSERT(maxClntPeers >= numClntPeers);
                unsigned numAvail = maxClntPeers - numClntPeers;
                P2pSrvrInfo info{peer->getRmtAddr(), tier, numAvail, SysClock::now()};
                tracker.insert(info);
            }
        }
        return tracker;
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
        return peerSrvr->accept(*this);
    }

    void startThreads() override {
        LOG_ASSERT(!stateMutex.try_lock());
        LOG_ASSERT(state == State::INIT);

        try {
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
        LOG_ASSERT(!stateMutex.try_lock());
        LOG_ASSERT(state == State::STOPPING);

        peerSrvr->halt(); // Causes `accept()` to return a false peer
        {
            Guard guard{peerSetMutex};
            tracker.halt();           // Causes connect thread to terminate
            peerSetCond.notify_all(); // Causes improve and connect threads to terminate
        } // `peerSetMutex` must be unlocked for some threads to terminate

        improveThread.join();
        acceptThread.join();
        connectThread.join();
    }

    void incPeerCount(Peer::Pimpl peer) override {
        LOG_ASSERT(!peerSetMutex.try_lock());
        peer->isClient() ? ++numClntPeers : ++numSrvrPeers;
    }

    void decPeerCount(Peer::Pimpl peer) override {
        LOG_ASSERT(!peerSetMutex.try_lock());
        peer->isClient() ? --numClntPeers : --numSrvrPeers;
    }

    void peerAdded(Peer::Pimpl peer) override {
        LOG_ASSERT(!peerSetMutex.try_lock());

        if (!peer->isClient()) {
            // Update the remote peer-manager about the local P2P-server
            P2pSrvrInfo srvrInfo;
            {
                Lock lock{peerSetMutex};
                srvrInfo.srvrAddr = peerSrvr->getSrvrAddr(),
                srvrInfo.numAvail = maxClntPeers - numClntPeers;
                srvrInfo.tier = tier;
            }
            peer->add(srvrInfo);
        }

        peer->add(tracker);
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
                  const unsigned      maxPeers,
                  const unsigned      evalTime)
        : P2pMgrImpl(subNode, tracker, ((maxPeers+1)/2)*2, (maxPeers+1)/2, evalTime)
        , subNode(subNode)
        , maxClntPeers(maxSrvrPeers)
        , numClntPeers(0)
        , timeout(timeout)
        , peerSrvr(SubPeerSrvr::create(peerConnSrvr))
        , connectThread()
        , tier(-1)
    {
        bookkeeper = Bookkeeper::createSub(maxPeers);
        peerSrvrInfo = P2pSrvrInfo{peerSrvr->getSrvrAddr(), tier, maxPeers, SysClock::now()};
        // Ensure that this instance doesn't try to connect to itself
        tracker.erase(peerSrvr->getSrvrAddr());
    }

    /// Copy constructs
    SubP2pMgrImpl(const SubP2pMgrImpl& impl) =delete;

    /// Copy assignment operator
    SubP2pMgrImpl& operator=(const SubP2pMgrImpl& rhs) =delete;

    void run() {
        P2pMgrImpl::run();
    }

    void halt() {
        P2pMgrImpl::halt();
    }

    /**
     * Returns current information on the P2P-server.
     * @return Current information on the P2P-server
     */
    P2pSrvrInfo getSrvrInfo() override {
        Guard guard{peerSetMutex};
        peerSrvrInfo.tier = tier;
        peerSrvrInfo.numAvail = maxSrvrPeers - numSrvrPeers;
        peerSrvrInfo.valid = SysClock::now();
        return peerSrvrInfo;
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
        Guard guard{peerSetMutex};
        if (peerMap.count(rmtAddr)) {
            auto& peer = peerMap[rmtAddr];
            reassign(peerMap[rmtAddr], prodId);
        }
    }
    void missed(const DataSegId dataSegId,
                SockAddr        rmtAddr) {
        Guard guard{peerSetMutex};
        if (peerMap.count(rmtAddr)) {
            auto& peer = peerMap[rmtAddr];
            reassign(peerMap[rmtAddr], dataSegId);
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
        const auto id = datum.getId();
        Guard      guard{peerSetMutex};

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
        const unsigned            maxPeers,
        const unsigned            evalTime) {
    return Pimpl{new SubP2pMgrImpl{subNode, tracker, peerConnSrvr, timeout, maxPeers,
            evalTime}};
}

} // namespace
