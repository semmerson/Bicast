/**
 * Creates and manages a peer-to-peer network.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: P2pNet.cpp
 *  Created on: Jul 1, 2019
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "P2pMgr.h"

#include "Bookkeeper.h"
#include "error.h"
#include "NodeType.h"
#include "PeerFactory.h"
#include "Thread.h"

#include <chrono>
#include <climits>
#include <condition_variable>
#include <cstring>
#include <exception>
#include <mutex>
#include <sstream>
#include <thread>
#include <unistd.h>

namespace hycast {

/**
 * Abstract base class implementation of a manager of a peer-to-peer network.
 */
class P2pMgr::Impl : public PeerSetMgr
{
    /**
     * Improves the set of peers by periodically stopping the worst-performing
     * peer -- providing the set is full and sufficient time has elapsed.
     * Executes on a new thread.
     *
     * @cancellationpoint  Yes
     */
    void improve()
    {
        LOG_DEBUG("Improving P2P network");
        try {
            Lock lock{stateMutex};

            for (;;) {
                static auto timeout = std::chrono::seconds{timePeriod};

                for (auto time = improveStart + timeout;
                        peerSet.size() < maxPeers || Clock::now() < time;
                        time = improveStart + timeout)
                    stateCond.wait_until(lock, time); // Cancellation point

                Peer peer = getBookkeeper().getWorstPeer();
                if (peer) {
                    peer.halt();
                    resetImprovement();        // Not a cancellation point
                }
            }
        }
        catch (const std::exception& ex) {
            setException(std::make_exception_ptr(ex));
        }
    }

protected:
    typedef std::mutex                Mutex;
    typedef std::lock_guard<Mutex>    Guard;
    typedef std::unique_lock<Mutex>   Lock;
    typedef std::condition_variable   Cond;
    typedef std::exception_ptr        ExceptPtr;
    typedef std::chrono::steady_clock Clock;

    std::atomic_flag  executing;     ///< Has `operator()` been called?
    mutable Mutex     stateMutex;    ///< Guards state of this instance
    mutable Mutex     doneMutex;     ///< Guards `doneCond`
    mutable Cond      doneCond;      ///< Instance should stop?
    mutable Cond      stateCond;     ///< For changes to this instance's state
    bool              haltRequested; ///< Termination requested?
    unsigned          timePeriod;    ///< Improvement period in seconds
    const int         maxPeers;      ///< Maximum number of peers
    P2pSndr&          p2pSndr;       ///< Peer-to-peer sender
    ExceptPtr         taskException; ///< Exception that terminated execution
    PeerSet           peerSet;       ///< Set of active peers
    Clock::time_point improveStart;  ///< Time of start of improvement period
    std::thread       acceptThread;  ///< Accepts remote peers
    std::thread       improveThread; ///< Improves set of peers

    virtual PeerFactory& getFactory() =0;

    virtual Bookkeeper& getBookkeeper() =0;

    /**
     * Sets the exception that caused this instance to terminate. Will only set
     * it once.
     *
     * @param[in] exPtr  Relevant exception pointer
     */
    void setException(const std::exception_ptr exPtr)
    {
        LOG_TRACE();
        Guard guard{doneMutex};

        if (!taskException) {
            taskException = exPtr; // No throw
            doneCond.notify_one(); // No throw
        }
    }

    /**
     * Resets the mechanism for improving the P2P network. Notifies `stateCond`.
     *
     * @pre               State is locked
     * @cancellationpoint No
     */
    void resetImprovement() {
        assert(!stateMutex.try_lock());
        getBookkeeper().resetCounts();
        improveStart = Clock::now();
        stateCond.notify_one();
    }

    /**
     * Adds a peer to the set of active peers.
     *
     * @pre                     `stateMutex` is locked
     * @param[in] peer          Peer to add
     * @throws    RuntimeError  Peer couldn't be added
     */
    void add(Peer peer)
    {
        assert(!stateMutex.try_lock());
        assert(peer);
        getBookkeeper().add(peer);

        try {
            (void)peerSet.activate(peer); // Fast
            resetImprovement();
            LOG_NOTE("Added peer %s", peer.getRmtAddr().to_string().c_str());
        }
        catch (const std::exception& ex) {
            getBookkeeper().erase(peer);
            std::throw_with_nested(RUNTIME_ERROR("Couldn't add peer " +
                    peer.getRmtAddr().to_string()));
        }
    }

    /**
     * Adds a peer, maybe. Implementation-specific.
     *
     * @param[in] peer         Peer to be potentially activated and added to
     *                         active peer-set
     * @retval    `true`       Peer was activated and added
     * @retval    `false`      Peer was not activated and added
     * @threadsafety           Safe
     * @exceptionsafety        Strong guarantee
     */
    virtual bool maybeAdd2(Peer peer) =0;

    /**
     * Adds a peer, maybe.
     *
     * @param[in] peer         Peer to be potentially activated and added to
     *                         active peer-set
     * @retval    `true`       Peer was activated and added
     * @retval    `false`      Peer was not activated and added
     * @threadsafety           Safe
     * @exceptionsafety        Strong guarantee
     */
    bool maybeAdd(Peer peer)
    {
        /*
         * TODO: Add a remote peer when this instance
         *   - Has fewer than `maxPeers`; or
         *   - Has `maxPeers`, is not the source site, and
         *       - The new site has a path to the source and most sites in the
         *         peer-set don't (=> replace worst peer that doesn't have a
         *         path to the source); or
         *       - The new site doesn't have a path to the source and
         *         most remote sites in the peer-set have a path to the source
         *         (=> replace worst peer that has a path to the source)
         */

        bool  success;
        {
            Guard guard{stateMutex};
            auto  numPeers = peerSet.size();

            if (numPeers < maxPeers) {
                add(peer);
                success = true;
            }
            else if (numPeers > maxPeers) {
                LOG_INFO("Peer %s wasn't added because peer-set is over-full",
                        peer.getRmtAddr().to_string().c_str());
                success = false;
            }
            else {
                success = maybeAdd2(peer); // Implementation-specific
            }
        }

        return success;
    }

    /**
     * Indicates whether or not this instance should terminate due to an
     * exception.
     *
     * @param[in] ex       The exception
     * @retval    `true`   This instance should terminate
     * @retval    `false`  This instance should not terminate
     */
    bool isFatal(const std::exception& ex)
    {
        try {
            std::rethrow_if_nested(ex);
        }
        catch (const std::system_error& sysEx) { // Must be before runtime_error
            const auto errCond = sysEx.code().default_error_condition();

            if (errCond.category() == std::generic_category()) {
                const auto errNum = errCond.value();

                //LOG_DEBUG("errNum: %d", errNum);
                return errNum != ECONNREFUSED &&
                       errNum != ECONNRESET &&
                       errNum != ENETUNREACH &&
                       errNum != ENETRESET &&
                       errNum != ENETDOWN &&
                       errNum != EHOSTUNREACH;
            }
        }
        catch (const std::runtime_error& ex) {
            //LOG_DEBUG("Non-fatal error: %s", ex.what());
            return false; // Simple EOF
        }
        catch (const std::exception& innerEx) {
            return isFatal(innerEx);
        }

        //LOG_DEBUG("Fatal error: %s", ex.what());
        return true;
    }

    /**
     * Waits until conditions are ripe for connecting to a remote peer-server.
     *
     * @cancellationpoint
     */
    void waitToConnect()
    {
        try {
            Lock lock{stateMutex};

            while (peerSet.size() >= maxPeers) {
                //LOG_DEBUG("peerSet.size(): %zu", peerSet.size());
                stateCond.wait(lock);
            }
        } catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't wait to connect"));
        }
    }

    /**
     * Accepts incoming connections from remote peers and attempts to add the
     * resulting local peer to the set of active peers. Executes on a new
     * thread.
     *
     * @cancellationpoint  Yes
     */
    virtual void accept() =0;

    /**
     * Starts implementation-specific tasks.
     */
    virtual void startTasks2() =0;

    void startImprover() {
        //LOG_DEBUG("Creating \"improvement\" thread");
        improveThread = std::thread(&Impl::improve, this);
    }

    void stopImprover() {
        if (improveThread.joinable()) {
            int status = ::pthread_cancel(improveThread.native_handle());
            if (status)
                throw SYSTEM_ERROR("Couldn't cancel \"improvement\" thread",
                        status);
            improveThread.join();
        }
    }

    void startAccepter() {
        //LOG_DEBUG("Creating \"accept\" thread");
        acceptThread = std::thread(&Impl::accept, this);
    }

    void stopAccepter() {
        if (acceptThread.joinable()) {
            getFactory().close(); // Causes `factory.accept()` to return
            acceptThread.join();
        }
    }

    /**
     * Starts the tasks of this instance on new threads.
     */
    void startTasks()
    {
        startAccepter();

        try {
            startTasks2(); // Implementation-specific tasks
        }
        catch (const std::exception& ex) {
            stopAccepter();
            throw;
        }
    }

    /**
     * Stops implementation-specific tasks.
     */
    virtual void stopTasks2() =0;

    /**
     * Stops the tasks of this instance.
     */
    void stopTasks()
    {
        try {
            stopTasks2(); // Implementation-specific tasks
            stopAccepter();
            stopImprover();
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't stop tasks"));
        }
    }

    /**
     * Waits until this instance should stop.
     */
    void waitUntilDone()
    {
        Lock lock(doneMutex);

        while (!haltRequested && !taskException)
            doneCond.wait(lock);
    }

public:
    /**
     * Constructs. Calls `::listen()`. No peers are managed until `operator()()`
     * is called.
     *
     * @param[in] maxPeers  Maximum number of peers
     * @param[in] p2pSndr   Peer-to-peer sender. Must exist for the duration of
     *                      this instance
     */
    Impl(   const int maxPeers,
            P2pSndr&  p2pSndr)
        : executing{ATOMIC_FLAG_INIT}
        , stateMutex{}
        , doneMutex{}
        , doneCond{}
        , stateCond{}
        , haltRequested{false}
        , timePeriod{60}
        , maxPeers{maxPeers}
        , p2pSndr(p2pSndr)
        , taskException{}
        , peerSet{*this}
        , improveStart{Clock::now()}
        , improveThread{}
        , acceptThread{}
    {}

    ~Impl() noexcept
    {
        LOG_TRACE();
        try {
            Guard guard(doneMutex);
            stopTasks();
        }
        catch (const std::exception& ex) {
            log_error(ex);
        }
        LOG_TRACE();
    }

    void setTimePeriod(unsigned timePeriod)
    {
        Guard guard{stateMutex};
        this->timePeriod = timePeriod;
    }

    /**
     * Executes this instance. Returns if
     *   - `halt()` is called
     *   - An exception is thrown
     * Returns immediately if `halt()` was called before this method.
     *
     * @threadsafety     Safe
     * @exceptionsafety  No guarantee
     */
    void operator ()()
    {
        try {
            if (executing.test_and_set())
                throw LOGIC_ERROR("Already called");

            startTasks();

            try {
                waitUntilDone();
                stopTasks();

                Guard guard(doneMutex);

                //LOG_DEBUG("haltRequested: %s", haltRequested ? "true" : "false");
                //LOG_DEBUG("taskException: %s", taskException ? "set" : "not set");
                if (!haltRequested && taskException)
                    std::rethrow_exception(taskException);
            }
            catch (const std::exception& ex) {
                LOG_DEBUG("Caught \"%s\"", ex.what());
                stopTasks();
                throw;
            }
            catch (...) {
                LOG_DEBUG("Caught ...");
                stopTasks();
                throw;
            }
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Failure executing "
                    "peer-to-peer manager"));
        }
    }

    /**
     * Halts execution of this instance. If called before `operator()`, then
     * this instance will never execute.
     *
     * @threadsafety     Safe
     * @exceptionsafety  Basic guarantee
     */
    void halt()
    {
        LOG_TRACE();
        Guard guard(doneMutex);

        haltRequested = true;
        doneCond.notify_one();
    }

    /**
     * Returns the number of active peers.
     *
     * @return Number of active peers
     */
    size_t size() const
    {
        return peerSet.size();
    }

    /**
     * Notifies all remote peers about available product-information.
     *
     * @param[in] prodIndex  Identifier of product
     */
    void notify(ProdIndex prodIndex)
    {
        return peerSet.notify(prodIndex);
    }

    /**
     * Notifies all remote peers about an available data-segment.
     *
     * @param[in] segId  Identifier of data-segment
     */
    void notify(const SegId& segId)
    {
        return peerSet.notify(segId);
    }

    /**
     * Obtains product-information for a remote peer.
     *
     * @param[in] prodIndex  Identifier of product
     * @param[in] peer       Peer
     * @return               The information. Will be empty if it doesn't exist.
     */
    virtual ProdInfo getProdInfo(
            Peer&           peer,
            ProdIndex       prodIndex) =0;

    /**
     * Obtains a data-segment for a remote peer.
     *
     * @param[in] segId      Identifier of the data-segment
     * @param[in] peer       Peer
     * @return               The segment. Will be empty if it doesn't exist.
     */
    virtual MemSeg getMemSeg(
            Peer&           peer,
            const SegId&    segId) =0;

    virtual void stopped2(Peer peer) =0;

    /**
     * Handles a stopped peer. Called by `peerSet`.
     *
     * @param[in] peer              The peer that stopped
     * @throws    std::logic_error  `peer` not found in performance map
     */
    void stopped(Peer peer)
    {
        const SockAddr& rmtSockAddr{peer.getRmtAddr()};

        LOG_NOTE("Peer %s stopped", rmtSockAddr.to_string().c_str());
        {
            Guard guard{stateMutex};

            stopped2(peer); // Implementation-specific
            getBookkeeper().erase(peer);
            resetImprovement();    // Restart performance evaluation
        }
    }
};

/******************************************************************************/

class PubP2pMgr final : public P2pMgr::Impl, public SendPeerMgr
{
    PubPeerFactory factory;    ///< Creates peers
    PubBookkeeper  bookkeeper; ///< Keeps track of peer performance

protected:
    void startTasks2() {
        if (maxPeers > 1) {
            startImprover();
        }
    }

    bool maybeAdd2(Peer peer) {
        return false;
    }

    /**
     * Accepts incoming connections from remote peers and attempts to add the
     * resulting local peer to the set of active peers. Executes on a new
     * thread.
     *
     * @cancellationpoint  Yes
     */
    void accept()
    {
        //LOG_DEBUG("Accepting peers");
        try {
            for (;;) {
                //LOG_DEBUG("Accepting connection");
                Peer peer = factory.accept(); // Potentially slow

                if (!peer)
                    break; // `factory.close()` called

                (void)maybeAdd(peer);
            }
        }
        catch (const std::exception& ex) {
            //LOG_DEBUG(ex, "Caught std::exception");
            setException(std::make_exception_ptr(ex));
        }
    }

    PeerFactory& getFactory() {
        return factory;
    }

    Bookkeeper& getBookkeeper() {
        return bookkeeper;
    }

    void stopTasks2() {
    }

public:
    PubP2pMgr(
            P2pInfo&  p2pInfo,
            P2pSndr&   p2pPub)
        : P2pMgr::Impl(p2pInfo.maxPeers, p2pPub)
        , factory{p2pInfo.sockAddr, p2pInfo.listenSize, p2pInfo.portPool, *this}
        , bookkeeper(p2pInfo.maxPeers)
    {}

    /**
     * Obtains product-information for a remote peer.
     *
     * @param[in] prodIndex  Identifier of product
     * @param[in] peer       Peer
     * @return               The information. Will be empty if it doesn't exist.
     */
    ProdInfo getProdInfo(
            Peer&           peer,
            ProdIndex       prodIndex)
    {
        auto prodInfo = p2pSndr.getProdInfo(prodIndex);
        bookkeeper.requested(peer, prodInfo);
        return prodInfo;
    }

    /**
     * Obtains a data-segment for a remote peer.
     *
     * @param[in] segId      Identifier of the data-segment
     * @param[in] peer       Peer
     * @return               The segment. Will be empty if it doesn't exist.
     */
    MemSeg getMemSeg(
            Peer&           peer,
            const SegId&    segId)
    {
        auto memSeg = p2pSndr.getMemSeg(segId);
        bookkeeper.requested(peer, memSeg.getSegInfo());
        return memSeg;
    }

    /**
     * Handles a stopped peer. Called by `peerSet`.
     *
     * @param[in] peer              The peer that stopped
     */
    void stopped2(Peer peer) {
    }
};

/******************************************************************************/

class SubP2pMgr final : public P2pMgr::Impl, public XcvrPeerMgr
{
    SubPeerFactory factory;       ///< Creates peers
    SubBookkeeper  bookkeeper;    ///< Keeps track of peer performance
    NodeType       lclNodeType;   ///< Current type of local node
    std::thread    connectThread; ///< Accepts incoming connections
    ServerPool     serverPool;    ///< Pool of potential remote peer-servers
    P2pSub&        p2pSub;        ///< Peer-to-peer subscriber

    /**
     * Episodically connects to a remote peer-server from the pool of such
     * servers to create a new peer and adds it to the set of peers if
     * possible. Executes on a new thread.
     *
     * @cancellationpoint  Yes
     */
    void connect()
    {
        //LOG_DEBUG("Connecting to peers");
        try {
            for (;;) {
                waitToConnect(); // Cancellation point

                // Cancellation point
                SockAddr srvrAddr = serverPool.pop(); // May block

                try {
                    LOG_DEBUG("Connecting to %s", srvrAddr.to_string().c_str());
                    // Cancellation point
                    Peer peer = factory.connect(srvrAddr, lclNodeType); // Potentially slow

                    {
                        Canceler canceler{false};
                        if (!maybeAdd(peer))
                            serverPool.consider(srvrAddr, timePeriod);
                    }
                }
                catch (const std::system_error& sysEx) {
                    const auto errCond = sysEx.code().default_error_condition();

                    if (errCond.category() == std::generic_category()) {
                        const auto errNum = errCond.value();

                        //LOG_DEBUG("errNum: %d", errNum);
                        if (errNum != ECONNREFUSED &&
                            errNum != ECONNRESET &&
                            errNum != ENETUNREACH &&
                            errNum != ENETRESET &&
                            errNum != ENETDOWN &&
                            errNum != EHOSTUNREACH)
                            throw;
                    }
                    serverPool.consider(srvrAddr, timePeriod);
                }
                catch (const std::exception& ex) {
                    log_note(ex);
                    serverPool.consider(srvrAddr, timePeriod);
                }
            } // Indefinite loop
        }
        catch (const std::exception& ex) {
            //LOG_DEBUG("Caught std::exception");
            setException(std::make_exception_ptr(ex));
        }
        catch (...) {
            //LOG_DEBUG("Caught ... exception");
            throw;
        }
    }

    void startTasks2() {
        if (!serverPool.empty()) {
            //LOG_DEBUG("Creating \"connect\" thread");
            connectThread = std::thread(&SubP2pMgr::connect, this);
        }

        try {
            if (maxPeers > 1 && !serverPool.empty())
                startImprover();
        }
        catch (const std::exception& ex) {
            if (connectThread.joinable()) {
                int status = ::pthread_cancel(connectThread.native_handle());
                if (status)
                    throw SYSTEM_ERROR("Couldn't cancel \"connect\" thread",
                            status);
                connectThread.join();
            }
            throw;
        }
    }

    void stopTasks2() {
        if (connectThread.joinable()) {
            int status = ::pthread_cancel(connectThread.native_handle());
            if (status)
                throw SYSTEM_ERROR("Couldn't cancel \"connect\" thread",
                        status);
        }
    }

    bool maybeAdd2(Peer peer) {
        bool       success = false;
        unsigned   numPath, numNoPath;
        const bool isPathToPub = peer.isPathToPub();

        bookkeeper.getPubPathCounts(numPath, numNoPath);

        if ((numPath < numNoPath) == isPathToPub) {
            Peer worst = bookkeeper.getWorstPeer(isPathToPub);

            if (worst) {
                worst.halt();
                add(peer);
                success = true;
            }
        }

        return success;
    }

    /**
     * Accepts incoming connections from remote peers and attempts to add the
     * resulting local peer to the set of active peers. Executes on a new
     * thread.
     *
     * @cancellationpoint  Yes
     */
    void accept()
    {
        //LOG_DEBUG("Accepting peers");
        try {
            for (;;) {
                //LOG_DEBUG("Accepting connection");
                Peer peer = factory.accept(lclNodeType); // Potentially slow

                if (!peer)
                    break; // `factory.close()` called

                (void)maybeAdd(peer);
            }
        }
        catch (const std::exception& ex) {
            //LOG_DEBUG(ex, "Caught std::exception");
            setException(std::make_exception_ptr(ex));
        }
    }

    /**
     * Reassigns a stopped peer's outstanding requests to the next-best peers in
     * the peer-set. For each request, if no peer in the set has been notified
     * about the associated item, then the request is discarded in the
     * expectation that one of the other remote peers will, eventually, notify.
     * a local peer about the available item.
     *
     * @pre             The state is locked
     * @param[in] peer  The stopped peer
     */
    void reassignPending(Peer& peer)
    {
        auto& chunkIds = bookkeeper.getRequested(peer);
        for (auto chunkId : chunkIds) {
            Peer altPeer = bookkeeper.popBestAlt(chunkId);
            if (altPeer) {
                chunkId.request(altPeer);
                bookkeeper.requested(altPeer, chunkId);
            }
        }
    }

    /**
     * Handles a stopped peer. Called by `peerSet`.
     *
     * @param[in] peer              The peer that stopped
     * @throws    std::logic_error  `peer` not found in performance map
     */
    void stopped2(Peer peer)
    {
        /*
         * Add the remote site to the pool of remote sites if the local peer
         * resulted from a `::connect`.
         */
        if (peer.isFromConnect())
            serverPool.consider(peer.getRmtAddr(), timePeriod);

        reassignPending(peer); // Reassign peer's outstanding requests
    }

    PeerFactory& getFactory() {
        return factory;
    }

    Bookkeeper& getBookkeeper() {
        return bookkeeper;
    }

public:
    SubP2pMgr(
            P2pInfo&    p2pInfo,
            ServerPool& serverPool,
            P2pSub&     p2pSub)
        : P2pMgr::Impl(p2pInfo.maxPeers, p2pSub)
        , factory{p2pInfo.sockAddr, p2pInfo.listenSize, p2pInfo.portPool, *this}
        , bookkeeper(maxPeers)
        , serverPool{serverPool}
        , p2pSub(p2pSub)
    {}

    /**
     * Handles a remote node transitioning from not having a path to the source
     * of data-products to having one. Might be called by a peer only *after*
     * `Peer::operator()()` is called.
     *
     * @param[in]     peer  Local peer whose remote peer transitioned
     * @threadsafety  Safe
     */
    void pathToPub(Peer& peer)
    {
        Guard    guard(stateMutex);
        unsigned numWithPath, numWithoutPath;

        bookkeeper.getPubPathCounts(numWithPath, numWithoutPath);
        if (numWithPath == 1) {
            lclNodeType = NodeType::PATH_TO_PUBLISHER;
            peerSet.gotPath(peer);
        }
    }

    /**
     * Handles a remote node transitioning from having a path to the source of
     * data-products to not having one. Might be called by a peer only *after*
     * `Peer::operator()()` is called.
     *
     * @param[in]     peer  Local peer whose remote peer transitioned
     * @threadsafety  Safe
     */
    void noPathToPub(Peer& peer)
    {
        Guard    guard(stateMutex);
        unsigned numWithPath, numWithoutPath;

        bookkeeper.getPubPathCounts(numWithPath, numWithoutPath);
        if (numWithPath == 0) {
            lclNodeType = NodeType::NO_PATH_TO_PUBLISHER;
            peerSet.lostPath(peer);
        }
    }

    /**
     * Indicates if product-information should be requested from a remote peer.
     *
     * @param[in] peer       Peer making the offer
     * @param[in] prodIndex  Identifier of the product
     * @retval    `true`     Product-information should be requested
     * @retval    `false`    Product-information should not be requested
     */
    bool shouldRequest(
            Peer&           peer,
            ProdIndex       prodIndex)
    {
        const bool should = p2pSub.shouldRequest(prodIndex) &&
                bookkeeper.shouldRequest(peer, prodIndex); // Thread safe

        LOG_DEBUG("Product-information %s %s be requested",
                prodIndex.to_string().data(), should ? "should" : "shouldn't");

        return should;
    }

    /**
     * Indicates if a data-segment should be requested from a remote peer.
     *
     * @param[in] peer     Peer making the offer
     * @param[in] segId    Identifier of the data-segment
     * @retval    `true`   The segment should be requested from the peer
     * @retval    `false`  The segment should not be requested from the peer
     */
    bool shouldRequest(
            Peer&           peer,
            const SegId&    segId)
    {
        Guard      guard{stateMutex};
        const bool should = p2pSub.shouldRequest(segId) &&
                bookkeeper.shouldRequest(peer, segId); // Thread safe

        LOG_DEBUG("Data-segment %s %s be requested",
                segId.to_string().data(), should ? "should" : "shouldn't");

        return should;
    }

    /**
     * Processes product-information from a peer.
     *
     * @param[in] peer      Providing peer
     * @param[in] prodInfo  Product information
     * @retval    `true`    Information was accepted
     * @retval    `false`   Information was previously accepted
     */
    bool hereIs(
            Peer&           peer,
            const ProdInfo& prodInfo)
    {
        if (!bookkeeper.received(peer, prodInfo.getProdIndex()))
            return false; // Wasn't requested

        if (!p2pSub.hereIsP2p(prodInfo))
            return false; // Wasn't needed

        peerSet.notify(prodInfo.getProdIndex(), peer);
        return true;
    }

    /**
     * Processes a data-segment from a peer.
     *
     * @param[in] peer     Providing peer
     * @param[in] seg      The data-segment
     * @retval    `true`   Chunk was accepted
     * @retval    `false`  Chunk was previously accepted
     */
    bool hereIs(
            Peer&           peer,
            TcpSeg&         seg)
    {
        if (!bookkeeper.received(peer, seg.getSegId()))
            return false; // Wasn't requested

        if (!p2pSub.hereIs(seg))
            return false; // Wasn't needed

        peerSet.notify(seg.getSegId(), peer);
        return true;
    }

    /**
     * Obtains product-information for a remote peer.
     *
     * @param[in] prodIndex  Identifier of product
     * @param[in] peer       Peer
     * @return               The information. Will be empty if it doesn't exist.
     */
    ProdInfo getProdInfo(
            Peer&           peer,
            ProdIndex       prodIndex)
    {
        return p2pSndr.getProdInfo(prodIndex);
    }

    /**
     * Obtains a data-segment for a remote peer.
     *
     * @param[in] segId      Identifier of the data-segment
     * @param[in] peer       Peer
     * @return               The segment. Will be empty if it doesn't exist.
     */
    MemSeg getMemSeg(
            Peer&           peer,
            const SegId&    segId)
    {
        return p2pSndr.getMemSeg(segId);
    }
};

/******************************************************************************/

P2pMgr::P2pMgr()
    : pImpl{} {
}

P2pMgr::P2pMgr(
        P2pInfo&  p2pInfo,
        P2pSndr&   p2pPub)
    : pImpl(new PubP2pMgr(p2pInfo, p2pPub)) {
}

P2pMgr::P2pMgr(
        P2pInfo&      p2pInfo,
        ServerPool&   p2pSrvrPool,
        P2pSub&       p2pSub)
    : pImpl(new SubP2pMgr(p2pInfo, p2pSrvrPool, p2pSub)) {
}

P2pMgr& P2pMgr::setTimePeriod(const unsigned timePeriod) {
    pImpl->setTimePeriod(timePeriod);
    return *this;
}

void P2pMgr::operator ()() {
    pImpl->operator()();
}

size_t P2pMgr::size() const {
    return pImpl->size();
}

void P2pMgr::notify(const ProdIndex prodIndex) const {
    return pImpl->notify(prodIndex);
}

void P2pMgr::notify(const SegId& segId) const {
    return pImpl->notify(segId);
}

void P2pMgr::halt() const {
    pImpl->halt();
}

} // namespace
