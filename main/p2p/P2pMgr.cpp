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
#include "HycastProto.h"
#include "Node.h"
#include "Peer.h"
#include "ThreadException.h"
#include "Tracker.h"
#include "Xprt.h"

#include <condition_variable>
#include <exception>
#include <list>
#include <pthread.h>
#include <set>
#include <system_error>
#include <unordered_map>

namespace hycast {

/******************************************************************************/

/// Base P2P manager implementation
class P2pMgrImpl : public P2pMgr
{
    Node&             node;          ///< Hycast node
    bool              peersChanged;  ///< Set of peers has changed?
    const unsigned    maxPeers;      ///< Maximum number of active peers
    Tracker           badP2pSrvrs;   ///< Bad (i.e., unavailable) P2P servers
    Tracker           goodP2pSrvrs;  ///< Good (i.e., connected) P2P servers

    /**
     * Removes a peer from this instance.
     *
     * @pre             Set-of-peers mutex is locked
     * @param[in] peer  Peer to be removed
     * @post            Set-of-peers mutex is locked
     */
    void remove(Peer::Pimpl peer, const bool isBad = false) {
        if (peerSet.erase(peer)) {
            const auto n = peerMap.erase(peer->getRmtAddr());
            LOG_ASSERT(n);
            const auto existed = bookkeeper->erase(peer);
            LOG_ASSERT(existed);
            decPeerCount(peer);
            if (isBad)
                badP2pSrvrs.insert(peer->getRmtAddr());
            goodP2pSrvrs.erase(peer->getRmtAddr());
            peersChanged = true;
            peerSetCond.notify_all();
        }
    }

    template<typename ID>
    void notify(const ID id) {
        Guard guard{peerSetMutex};
        for (auto iter = peerSet.begin(); iter != peerSet.end(); ) {
            auto peer = *(iter++); // Increment prevents becoming invalid
            if (shouldNotify(peer, id)) {
                if (!peer->notify(id)) {
                    // Connection lost
                    peer->stop();
                    remove(peer, true); // `true` => bad peer
                }
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
    mutable Mutex        peerSetMutex;  ///< To support concurrency
    mutable Cond         peerSetCond;   ///< To support concurrency
    ThreadEx             threadEx;      ///< Exception thrown by internal threads
    const unsigned       maxSrvrPeers;  ///< Maximum number of server-side peers
    unsigned             numSrvrPeers;  ///< Number of server-side peers
    PeerSet              peerSet;       ///< Set of peers
    PeerMap              peerMap;       ///< Map from remote address to peer
    Bookkeeper::Pimpl    bookkeeper;    ///< Monitors peer performance
    Thread               acceptThread;  ///< Creates server-side peers
    Thread               improveThread; ///< Improves the set of peers
    std::chrono::seconds evalTime;      ///< Evaluation interval for poorest-performing peer in
                                        ///< seconds

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
     * Adds a new peer and starts the peer.
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
        // TODO: fix this. Remote socket address isn't the P2P server's address, is it?
        peer->remove(badP2pSrvrs); // Tells remote about unavailable P2P servers
        peer->add(goodP2pSrvrs);   // Tells remote about available P2P servers

        return true;
    }

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
     * @pre             Mutex is locked
     * @param[in] peer  Peer to be removed
     * @post            Mutex is locked
     */
    virtual void decPeerCount(Peer::Pimpl peer) =0;

    /**
     * Indicates if a remote peer should be notified about available information
     * on a product.
     *
     * @param[in] peer       Peer
     * @param[in] prodId     Product identifier
     * @retval    `true`     Peer should be notified
     * @retval    `false`    Peer should not be notified
     */
    virtual bool shouldNotify(
            Peer::Pimpl peer,
            ProdId      prodId) =0;

    /**
     * Indicates if a remote peer should be notified about an available data
     * segment.
     *
     * @param[in] peer       Peer
     * @param[in] dataSegId  ID of the data segment
     * @retval    `true`     Peer should be notified
     * @retval    `false`    Peer should not be notified
     */
    virtual bool shouldNotify(
            Peer::Pimpl peer,
            DataSegId   dataSegId) =0;

public:
    /**
     * Constructs.
     *
     * @param[in] node          ///< Associated Hycast node
     * @param[in] maxPeers      ///< Maximum number of peers
     * @param[in] maxSrvrPeers  ///< Maximum number of server-side-constructed peers
     * @param[in] evalTime      ///< Peer evaluation time in seconds
     */
    P2pMgrImpl(
            Node&          node,
            const unsigned maxPeers,
            const unsigned maxSrvrPeers,
            const unsigned evalTime)
        : state(State::INIT)
        , stateMutex()
        , stateCond()
        , node(node)
        , peersChanged(false)
        , maxPeers(maxPeers)
        , badP2pSrvrs(maxPeers)
        , goodP2pSrvrs(maxPeers)
        , peerSetMutex()
        , peerSetCond()
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
     * Runs the P2P server. Accepts connecting remote peers and adds them to
     * the set of active server peers. Must be public because it's the start
     * function for a thread.
     *
     * NB: `noexcept` is incompatible with thread cancellation.
     */
    void acceptPeers() {
        try {
            auto addPeer = [&]{return numSrvrPeers < maxSrvrPeers;};
            for (;;) {
                {
                    Lock lock{peerSetMutex};
                    peerSetCond.wait(lock, addPeer);
                    /*
                     * The mutex is released immediately because  a server-side peer is only added
                     * by the current thread.
                     */
                }

                auto peer = accept();
                if (!add(peer)) {
                    LOG_WARNING("Peer %s is already being used", peer->to_string().data());
                }
                else {
                    LOG_NOTE("Accepted connection from peer %s", peer->to_string().data());
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
     * Improves the set of peers by periodically removing the worst-performing
     * peer and notifying the peer-adding threads.
     *
     * Must be public so that the associated thread object can call it.
     *
     * @param[in] numPeers    Number of peers variable
     * @param[in] maxPeers    Maximum number of peers
     */
    void improvePeers(
            unsigned&      numPeers,
            const unsigned maxPeers) {
        try {
            Lock lock{peerSetMutex};
            for (;;) {
                // Wait until a full set of peers is unchanged for the evaluation duration
                bookkeeper->reset();
                for (peersChanged = false;
                        peerSetCond.wait_for(lock, evalTime, [&]{return peersChanged;}) ||
                            numPeers < maxPeers;
                        peersChanged = false) {
                    bookkeeper->reset();
                }

                auto worstPeer = bookkeeper->getWorstPeer();
                if (worstPeer) {
                    worstPeer->stop();
                    remove(worstPeer);
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

    ProdInfo recvRequest(const ProdId    prodId,
                         const SockAddr  rmtAddr) override {
        return node.recvRequest(prodId);
    }

    DataSeg recvRequest(const DataSegId segId,
                        const SockAddr  rmtAddr) override {
        return node.recvRequest(segId);
    }
};

/******************************************************************************/

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

    /**
     * @pre             Mutex is locked
     */
    void incPeerCount(Peer::Pimpl peer) override {
        ++numSrvrPeers;
    }

    /**
     * @pre             Mutex is locked
     */
    void decPeerCount(Peer::Pimpl peer) override {
        --numSrvrPeers;
    }

    bool shouldNotify(
            Peer::Pimpl peer,
            ProdId      prodId) override {
        // Publisher's peer should always notify the remote peer
        return true;
    }

    bool shouldNotify(
            Peer::Pimpl      peer,
            DataSegId dataSegId) override {
        // Publisher's peer should always notify the remote peer
        return true;
    }

public:
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
        : P2pMgrImpl(pubNode, maxPeers, maxPeers, evalTime)
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

    void waitForSrvrPeer() override {
        P2pMgrImpl::waitForSrvrPeer();
    }

    void notify(const ProdId prodId) override {
        P2pMgrImpl::notify(prodId);
    }

    void notify(const DataSegId segId) override {
        P2pMgrImpl::notify(segId);
    }

    ProdInfo recvRequest(const ProdId   prodId,
                         const SockAddr rmtAddr) override {
        return P2pMgrImpl::recvRequest(prodId, rmtAddr);
    }

    DataSeg recvRequest(const DataSegId segId,
                        const SockAddr  rmtAddr) override {
        return P2pMgrImpl::recvRequest(segId, rmtAddr);
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

/******************************************************************************/

/// Subscribing P2P manager implementation
class SubP2pMgrImpl final : public SubP2pMgr, public P2pMgrImpl
{
    /**
     * Delay FIFO of addresses of unused P2P servers.
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

    SubNode&           subNode;       ///< Subscriber's node
    const unsigned     maxClntPeers;  ///< Maximum number of client-side peers
    unsigned           numClntPeers;  ///< Number of client-side peers
    int                timeout;       ///< Timeout, in ms, for connecting to remote P2P server
    Tracker            tracker;       ///< Socket addresses of unused P2P servers
    DelayFifo          delayFifo;     ///< Delay FIFO of unused P2P servers
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
                     * The peer-set mutex is released immediately because  a client-side peer can
                     * only be added by the current thread.
                     */
                }

                auto srvrAddr = tracker.removeHead(); // Blocks if empty
                try {
                    auto peer = Peer::create(*this, srvrAddr);
                    if (!add(peer))
                        throw LOGIC_ERROR("Already connected to " + srvrAddr.to_string());
                    LOG_NOTE("Connected to %s", srvrAddr.to_string().data());
                }
                catch (const SystemError& ex) {
                    auto errnum = ex.code().value();
                    if (errnum == ECONNREFUSED ||
                            errnum == ENETUNREACH ||
                            errnum == ETIMEDOUT ||
                            errnum == ECONNRESET ||
                            errnum == EHOSTUNREACH ||
                            errnum == ENETDOWN) {
                        LOG_NOTE(ex);
                        // Try again later
                        delayFifo.insert(srvrAddr);
                    }
                    else {
                        throw;
                    }
                }
            }
        }
        catch (const std::exception& ex) {
            setException(ex);
        }
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
        peer->isClient() ? ++numClntPeers : ++numSrvrPeers;
    }

    /**
     * @pre             Mutex is locked
     */
    void decPeerCount(Peer::Pimpl peer) override {
        peer->isClient() ? --numClntPeers : --numSrvrPeers;
    }

    bool shouldNotify(
            Peer::Pimpl  peer,
            const ProdId prodId) override {
        // Remote peer is subscriber & doesn't have the datum => notify
        return !peer->isRmtPub() && bookkeeper->shouldNotify(peer, prodId);
    }

    bool shouldNotify(
            Peer::Pimpl            peer,
            const DataSegId dataSegId) override {
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
        : P2pMgrImpl(subNode, ((maxPeers+1)/2)*2, (maxPeers+1)/2, evalTime)
        , subNode(subNode)
        , maxClntPeers(maxSrvrPeers)
        , numClntPeers(0)
        , timeout(timeout)
        , tracker(tracker)
        , delayFifo()
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

    void recvAdd(const SockAddr srvrAddr) override {
        this->tracker.insert(srvrAddr);
    }

    void recvAdd(const Tracker tracker) override {
        this->tracker.insert(tracker);
    }

    void recvRemove(const SockAddr p2pSrvr) override {
        this->tracker.erase(p2pSrvr);
    }
    void recvRemove(const Tracker tracker) override {
        this->tracker.erase(tracker);
    }

    bool recvNotice(const ProdId   prodId,
                    const SockAddr rmtAddr) {
        // Must not exist and not been previously requested
        return subNode.shouldRequest(prodId) &&
                bookkeeper->shouldRequest(peerMap.at(rmtAddr), prodId);
    }
    bool recvNotice(const DataSegId segId,
                    SockAddr        rmtAddr) {
        // Must not exist and not been previously requested
        return subNode.shouldRequest(segId) &&
                bookkeeper->shouldRequest(peerMap.at(rmtAddr), segId);
    }

    ProdInfo recvRequest(const ProdId   prodId,
                         const SockAddr rmtAddr) override {
        return P2pMgrImpl::recvRequest(prodId, rmtAddr);
    }

    DataSeg recvRequest(const DataSegId segId,
                        const SockAddr  rmtAddr) override {
        return P2pMgrImpl::recvRequest(segId, rmtAddr);
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
        auto peer = peerMap.at(rmtAddr);
        /*
         * The bookkeeper is accessed in 3 phases:
         *   1) The given peer's rating is increased if the reception is valid;
         *   2) The bookkeeper is queried as to whether a peer should notify its
         *      remote counterpart about the received datum before the relevant
         *      information is deleted from the bookkeeper; and
         *   3) The relevant bookkeeper entries are deleted.
         */
        if (bookkeeper->received(peer, id)) {
            subNode.recvP2pData(datum);
            // Deadlock will occur if the mutex is locked by `stopImpl()`
            Guard guard{peerSetMutex};
            for (auto p : peerSet) {
                if (shouldNotify(p, datum.getId()))
                    p->notify(id);
            }
            bookkeeper->erase(id); // No longer relevant
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
