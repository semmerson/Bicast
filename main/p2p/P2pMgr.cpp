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

PeerChngObs::~PeerChngObs() noexcept
{}

void PeerChngObs::added(Peer& peer)
{
    throw LOGIC_ERROR("Should be called");
}

void PeerChngObs::removed(Peer& peer)
{
    throw LOGIC_ERROR("Should be called");
}

P2pMgrObs::~P2pMgrObs() noexcept
{}

class P2pMgr::Impl : public PeerObs, public PeerSet::Observer
{
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
    PeerFactory       factory;       ///< Creates local peers
    const int         maxPeers;      ///< Maximum number of peers
    P2pMgrObs&        p2pMgrObs;     ///< Observer of this instance
    ExceptPtr         taskException; ///< Exception that terminated execution
    ServerPool        serverPool;    ///< Pool of potential remote peer-servers
    Bookkeeper        bookkeeper;    ///< Keeps track of peer behaviors
    PeerSet           peerSet;       ///< Set of active peers
    Clock::time_point improveStart;  ///< Time of start of improvement period
    std::thread       improveThread; ///< Improves set of peers
    std::thread       connectThread; ///< Connects to remote peer-servers
    std::thread       acceptThread;  ///< Accepts incoming connections

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
     * Resets the mechanism for improving the P2P network.
     *
     * @pre               State is locked
     * @cancellationpoint No
     */
    void resetImprovement() {
        bookkeeper.resetCounts();
        improveStart = Clock::now();
    }

    /**
     * Adds a new peer.
     *
     * @pre                     `stateMutex` is locked
     * @param[in] peer          Peer to add
     * @throws    RuntimeError  Peer couldn't be added
     */
    void add(
            Peer       peer,
            const bool fromConnect)
    {
        bookkeeper.add(peer, fromConnect);
        try {
            (void)peerSet.activate(peer); // Fast
        }
        catch (const std::exception& ex) {
            bookkeeper.erase(peer);
            std::throw_with_nested(RUNTIME_ERROR("Couldn't add peer " +
                    peer.getRmtAddr().to_string()));
        }
    }

    /**
     * Replaces the worst-performing peer with a new peer.
     *
     * @pre             `stateMutex` is locked
     * @param[in] peer  Replacement peer
     */
    void replaceWorst(
            Peer       peer,
            const bool fromConnect)
    {
        bookkeeper.getWorstPeer().halt();
        bookkeeper.add(peer, fromConnect);
        (void)peerSet.activate(peer); // Fast
    }

    /**
     * Adds a new peer, maybe.
     *
     * @param[in] peer         Peer to be potentially activated and added to
     *                         active peer-set
     * @param[in] fromConnect  Was the remote peer obtained via `::connect()`?
     * @retval    `true`       Peer was activated and added
     * @retval    `false`      Peer was not activated and added
     * @threadsafety           Safe
     * @exceptionsafety        Strong guarantee
     */
    bool maybeAdd(
            Peer       peer,
            const bool fromConnect = false)
    {
        /*
         * TODO: Add a remote peer when this instance
         *   - Has fewer than `maxPeers`; or
         *   - Has `maxPeers` and is not the source site; and
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
                add(peer, fromConnect);
                success = true;
            }
            else if (numPeers > maxPeers) {
                LOG_INFO("Peer %s wasn't added because peer-set is over-full",
                        peer.getRmtAddr().to_string().c_str());
                success = false;
            }
            else {
                bookkeeper.add(peer, fromConnect);

                try {
                    (void)peerSet.activate(peer); // Fast

                    LOG_DEBUG("Added peer %s",
                            peer.getRmtAddr().to_string().c_str());

                    success = true;
                    resetImprovement();
                    stateCond.notify_all();
                }
                catch (const std::exception& ex) {
                    bookkeeper.erase(peer);
                    std::throw_with_nested(RUNTIME_ERROR("Couldn't add peer " +
                            peer.getRmtAddr().to_string()));
                }
            }
        }

        if (success)
            p2pMgrObs.added(peer);

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
     * Improves the set of peers by periodically stopping the worst-performing
     * peer -- providing the set is full and sufficient time has elapsed.
     * Executes on a new thread.
     *
     * @cancellationpoint  Yes
     */
    void improve()
    {
        //LOG_DEBUG("Improving P2P network");
        try {
            Lock lock{stateMutex};

            for (;;) {
                static auto timeout = std::chrono::seconds{timePeriod};

                for (auto time = improveStart + timeout;
                        peerSet.size() < maxPeers || Clock::now() < time;
                        time = improveStart + timeout)
                    stateCond.wait_until(lock, time); // Cancellation point

                bookkeeper.getWorstPeer().halt();
                resetImprovement();        // Not a cancellation point
            }
        }
        catch (const std::exception& ex) {
            setException(std::make_exception_ptr(ex));
        }
    }

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
                    Peer peer = factory.connect(srvrAddr); // Potentially slow

                    {
                        Canceler canceler{false};
                        if (!maybeAdd(peer, true))
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

    /**
     * Starts the tasks of this instance on new threads.
     */
    void startTasks()
    {
        if (maxPeers > 1 && !serverPool.empty()) {
            //LOG_DEBUG("Creating \"improvement\" thread");
            improveThread = std::thread(&Impl::improve, this);
        }

        try {
            if (!serverPool.empty()) {
                //LOG_DEBUG("Creating \"connect\" thread");
                connectThread = std::thread(&Impl::connect, this);
            }

            try {
                //LOG_DEBUG("Creating \"accept\" thread");
                acceptThread = std::thread(&Impl::accept, this);
            } // Connect thread created
            catch (const std::exception& ex) {
                if (connectThread.joinable()) {
                    int status =
                            ::pthread_cancel(connectThread.native_handle());

                    if (status)
                        throw SYSTEM_ERROR("Couldn't cancel \"connect\" thread",
                                status);

                    connectThread.join();
                }

                throw;
            }
        } // Improvement thread possibly created
        catch (const std::exception& ex) {
            if (improveThread.joinable()) {
                int status = ::pthread_cancel(improveThread.native_handle());

                if (status)
                    throw SYSTEM_ERROR("Couldn't cancel \"improvement\" thread",
                            status);

                improveThread.join();
            }
            throw;
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
                bookkeeper.requested(altPeer.getRmtAddr(), chunkId);
            }
        }
    }

    /**
     * Stops the tasks of this instance.
     */
    void stopTasks()
    {
        try {
            factory.close(); // Causes `factory.accept()` to return

            int status;

            if (connectThread.joinable()) {
                status = ::pthread_cancel(connectThread.native_handle());
                if (status)
                    throw SYSTEM_ERROR("Couldn't cancel \"connect\" thread",
                            status);
            }

            if (improveThread.joinable()) {
                status = ::pthread_cancel(improveThread.native_handle());
                if (status)
                    throw SYSTEM_ERROR("Couldn't cancel \"improve\" thread",
                            status);
            }

            acceptThread.join();
            if (connectThread.joinable())
                connectThread.join();
            if (improveThread.joinable())
                improveThread.join();
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
     * @param[in] srvrAddr     Socket address of local server that accepts
     *                         connections from remote peers
     * @param[in] listenSize   Size of server's `::listen()` queue
     * @param[in] portPool     Pool of available port numbers
     * @param[in] maxPeers     Maximum number of peers
     * @param[in] serverPool   Pool of possible remote servers for remote peers.
     *                         If empty, then this instance is the source of
     *                         data-products.
     * @param[in] p2pMgrObs    Observer of this instance
     */
    Impl(   const SockAddr&  srvrAddr,
            const int        listenSize,
            PortPool&        portPool,
            const int        maxPeers,
            ServerPool&      serverPool,
            P2pMgrObs&       p2pMgrObs)
        : executing{ATOMIC_FLAG_INIT}
        , stateMutex{}
        , doneMutex{}
        , doneCond{}
        , stateCond{}
        , haltRequested{false}
        , timePeriod{60}
        , factory{srvrAddr, listenSize, portPool, *this, serverPool.empty()}
        , maxPeers{maxPeers}
        , p2pMgrObs(p2pMgrObs)
        , taskException{}
        , serverPool{serverPool}
        , bookkeeper(maxPeers)
        , peerSet{*this} // Must be destroyed before `peerStats`
        , improveStart{Clock::now()}
        , improveThread{}
        , connectThread{}
        , acceptThread{}
    {}

    /**
     * Constructs. Calls `::listen()`. No peers are managed until `operator()()`
     * is called.
     *
     * @param[in] p2pInfo      Peer-to-peer execution parameters
     * @param[in] p2pSrvrPool  Pool of remote P2P servers. If empty, then this
     *                         instance is the source of data-products.
     * @param[in] p2pMgrObs    Observer of this instance
     */
    Impl(   P2pInfo&    p2pInfo,
            ServerPool& p2pSrvrPool,
            P2pMgrObs&  p2pMgrObs)
        : Impl(p2pInfo.sockAddr, p2pInfo.listenSize, p2pInfo.portPool,
                p2pInfo.maxPeers, p2pSrvrPool, p2pMgrObs)
    {}

    ~Impl() noexcept
    {
        LOG_TRACE();
        try {
            Guard guard(doneMutex);

            if (acceptThread.joinable())
                LOG_ERROR("Accept-thread is joinable");

            if (connectThread.joinable())
                LOG_ERROR("Connect-thread is joinable");

            if (improveThread.joinable())
                LOG_ERROR("Improve-thread is joinable");
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
     * Handles a stopped peer. Called by `peerSet`.
     *
     * @param[in] peer              The peer that stopped
     * @throws    std::logic_error  `peer` not found in performance map
     */
    void stopped(Peer peer)
    {
        const SockAddr& rmtSockAddr{peer.getRmtAddr()};

        LOG_NOTE("Removing peer %s", rmtSockAddr.to_string().c_str());

        {
            Guard guard{stateMutex};

            // Possibly add the remote peer-server to the pool of remote servers
            bool fromConnect;
            try {
                fromConnect = bookkeeper.isFromConnect(peer);
            }
            catch (const std::exception& ex) {
                std::throw_with_nested(LOGIC_ERROR("Peer " +
                        rmtSockAddr.to_string() +
                        " not found in performance map"));
            }

            if (fromConnect)
                serverPool.consider(rmtSockAddr, timePeriod);

            reassignPending(peer); // Reassign peer's outstanding requests
            bookkeeper.erase(peer);
            resetImprovement();    // Restart performance evaluation
            stateCond.notify_all();
        }

        p2pMgrObs.removed(peer);
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
     * Indicates if product-information should be requested from a remote peer.
     *
     * @param[in] prodIndex  Identifier of the product
     * @param[in] rmtAddr    Socket address of the remote peer
     * @retval    `true`     Product-information should be requested
     * @retval    `false`    Product-information should not be requested
     */
    bool shouldRequest(
            ProdIndex       prodIndex,
            const SockAddr& rmtAddr)
    {
        Guard      guard{stateMutex};
        const bool should = p2pMgrObs.shouldRequest(prodIndex) &&
                bookkeeper.shouldRequest(rmtAddr, prodIndex);

        LOG_DEBUG("Product-information %s %s be requested",
                prodIndex.to_string().data(), should ? "should" : "shouldn't");

        return should;
    }

    /**
     * Indicates if a data-segment should be requested from a remote peer.
     *
     * @param[in] segId    Identifier of the data-segment
     * @param[in] rmtAddr  Socket address of the remote peer
     * @retval    `true`   The segment should be requested from the peer
     * @retval    `false`  The segment should not be requested from the peer
     */
    bool shouldRequest(
            const SegId&    segId,
            const SockAddr& rmtAddr)
    {
        Guard      guard{stateMutex};
        const bool should = p2pMgrObs.shouldRequest(segId) &&
                bookkeeper.shouldRequest(rmtAddr, segId);

        LOG_DEBUG("Data-segment %s %s be requested",
                segId.to_string().data(), should ? "should" : "shouldn't");

        return should;
    }

    /**
     * Obtains product-information for a remote peer.
     *
     * @param[in] prodIndex  Identifier of product
     * @param[in] rmtAddr    Socket address of the remote peer
     * @return               The information. Will be empty if it doesn't exist.
     */
    ProdInfo get(
            ProdIndex       prodIndex,
            const SockAddr& rmtAddr)
    {
        return p2pMgrObs.get(prodIndex);
    }

    /**
     * Obtains a data-segment for a remote peer.
     *
     * @param[in] segId      Identifier of the data-segment
     * @param[in] rmtAddr    Socket address of the remote peer
     * @return               The segment. Will be empty if it doesn't exist.
     */
    MemSeg get(
            const SegId&    segId,
            const SockAddr& rmtAddr)
    {
        return p2pMgrObs.get(segId);
    }

    /**
     * Processes product-information from a peer.
     *
     * @param[in] prodInfo  Product information
     * @param[in] rmtAddr   Socket address of the remote peer
     * @retval    `true`    Information was accepted
     * @retval    `false`   Information was previously accepted
     */
    bool hereIs(
            const ProdInfo& prodInfo,
            const SockAddr& rmtAddr)
    {
        const bool isNew = p2pMgrObs.hereIsP2p(prodInfo);
        bookkeeper.received(rmtAddr, prodInfo.getProdIndex());
        return isNew;
    }

    /**
     * Processes a data-segment from a peer.
     *
     * @param[in] seg      The data-segment
     * @param[in] rmtAddr  Socket address of the remote peer
     * @retval    `true`   Chunk was accepted
     * @retval    `false`  Chunk was previously accepted
     */
    bool hereIs(
            TcpSeg&         seg,
            const SockAddr& rmtAddr)
    {
        const bool isNew = p2pMgrObs.hereIs(seg);
        bookkeeper.received(rmtAddr, seg.getSegId());
        return isNew;
    }
};

/******************************************************************************/

P2pMgr::P2pMgr()
    : pImpl{}
{}

P2pMgr::P2pMgr(
        const SockAddr& srvrAddr,
        const int       listenSize,
        PortPool&       portPool,
        const int       maxPeers,
        ServerPool&     serverPool,
        P2pMgrObs&      p2pMgrObs)
    : pImpl{new Impl(srvrAddr, listenSize, portPool, maxPeers, serverPool,
            p2pMgrObs)}
{}

P2pMgr::P2pMgr(
        P2pInfo&    p2pInfo,
        ServerPool& p2pSrvrPool,
        P2pMgrObs&  p2pMgrObs)
    : pImpl{new Impl(p2pInfo, p2pSrvrPool, p2pMgrObs)}
{}

P2pMgr& P2pMgr::setTimePeriod(const unsigned timePeriod)
{
    pImpl->setTimePeriod(timePeriod);
    return *this;
}

void P2pMgr::operator ()()
{
    pImpl->operator()();
}

size_t P2pMgr::size() const
{
    return pImpl->size();
}

void P2pMgr::notify(ProdIndex prodIndex)
{
    return pImpl->notify(prodIndex);
}

void P2pMgr::notify(const SegId& segId)
{
    return pImpl->notify(segId);
}

void P2pMgr::halt() const
{
    pImpl->halt();
}

} // namespace
