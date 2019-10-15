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

#include "error.h"
#include "P2pMgr.h"
#include "PeerFactory.h"
#include "Thread.h"

#include <atomic>
#include <chrono>
#include <climits>
#include <condition_variable>
#include <cstring>
#include <exception>
#include <mutex>
#include <sstream>
#include <thread>
#include <unistd.h>
#include <unordered_map>

namespace hycast {

class P2pMgr::Impl : public PeerMsgRcvr, public PeerSet::Observer
{
    class PeerStats {
        typedef struct PeerStat {
            Peer          peer;        ///< Peer
            uint_fast32_t chunkCount;  ///< Number of received chunks
            bool          fromConnect; ///< From `::connect()`?

            PeerStat()
                : peer()
                , chunkCount(0)
                , fromConnect(false)
            {}

            PeerStat(
                    Peer       peer,
                    const bool fromConnect)
                : peer{peer}
                , chunkCount{0}
                , fromConnect{fromConnect}
            {}
        }                                       PeerStat;
        typedef std::mutex                      Mutex;
        typedef std::lock_guard<Mutex>          Guard;

        std::mutex                              mutex;
        std::unordered_map<SockAddr, PeerStat>  map;

    public:
        void add(
                Peer       peer,
                const bool fromConnect)
        {
            const SockAddr& rmtSockAddr = peer.getRmtAddr();

            LOG_NOTE("Adding peer %s", rmtSockAddr.to_string().c_str());

            Guard guard{mutex};
            map.emplace(rmtSockAddr, PeerStat(peer, fromConnect));
        }

        void erase(const SockAddr& rmtSockAddr)
        {
            Guard guard{mutex};
            map.erase(rmtSockAddr);
        }

        bool fromConnect(const SockAddr& rmtSockAddr)
        {
            Guard guard{mutex};
            return map.count(rmtSockAddr)
                    ? map[rmtSockAddr].fromConnect
                    : false;
        }

        void inc(const SockAddr& rmtSockAddr)
        {
            Guard guard{mutex};

            if (map.count(rmtSockAddr))
                ++map[rmtSockAddr].chunkCount;
        }

        /**
         * Stops the worst performing peer if it's unique and not tied with any
         * other peer.
         *
         * @cancellationpoint  No
         */
        void stopWorstPeer() {
            unsigned long minCount{ULONG_MAX};
            unsigned long tieCount{ULONG_MAX};
            Peer          peer;
            Guard         guard{mutex};

            for (auto iter = map.begin(), end = map.end(); iter != end;
                    ++iter) {
                auto count = iter->second.chunkCount;

                if (count < minCount) {
                    minCount = count;
                    peer = iter->second.peer;
                }
                else if (count == minCount) {
                    tieCount = minCount;
                }
            }

            if (peer && minCount != tieCount)
                peer.halt();
        }

        /**
         * Resets the chunk counts for all peers.
         *
         * @cancellationpoint No
         */
        void resetChunkCounts() {
            Guard         guard{mutex};

            for (auto iter = map.begin(), end = map.end(); iter != end; ++iter)
                iter->second.chunkCount = 0;
        }
    };
    //typedef std::unordered_map<Peer, PeerStat>      PeerStats;
    typedef std::mutex                              Mutex;
    typedef std::lock_guard<Mutex>                  Guard;
    typedef std::unique_lock<Mutex>                 Lock;
    typedef std::condition_variable                 Cond;
    typedef std::exception_ptr                      ExceptPtr;
    typedef std::chrono::steady_clock               Clock;

    std::atomic_flag  executing;     ///< Has `operator()` been called?
    mutable Mutex     peerSetMutex;  ///< Guards set of peers
    mutable Mutex     doneMutex;     ///< Guards `doneCond`
    mutable Cond      doneCond;      ///< Instance should stop?
    mutable Cond      peerSetCond;   ///< For changes to the set of peers
    bool              haltRequested; ///< Termination requested?
    unsigned          timePeriod;    ///< Improvement period in seconds
    PeerFactory       factory;       ///< Creates local peers
    const int         maxPeers;      ///< Maximum number of peers
    PeerMsgRcvr&      msgRcvr;       ///< Receiver of messages from remote peers
    ExceptPtr         taskException; ///< Exception that terminated execution
    ServerPool        serverPool;    ///< Pool of potential remote peer-servers
    PeerStats         peerStats;     ///< Peer performance statistics
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
     * @cancellationpoint No
     */
    void resetImprovement() {
        peerStats.resetChunkCounts();
        improveStart = Clock::now();
    }

    /**
     * @param[in] peer         Peer to be activated and added to active peer-set
     * @param[in] fromConnect  Was the remote peer obtained via `::connect()`?
     * @retval    `true`       Peer was activated and added
     * @retval    `false`      Peer was not activated and added because set is
     *                         full
     * @threadsafety           Safe
     * @exceptionsafety        Strong guarantee
     */
    bool add(
            Peer       peer,
            const bool fromConnect = false)
    {
        bool  success;
        Guard guard{peerSetMutex};

        if (peerSet.size() >= maxPeers) {
            LOG_INFO("Didn't add peer %s because peer-set is full",
                    peer.getRmtAddr().to_string().c_str());
            success = false;
        }
        else {
            peerStats.add(peer, fromConnect);

            const SockAddr rmtSockAddr{peer.getRmtAddr()};

            try {
                (void)peerSet.activate(peer); // Fast

                success = true;
                resetImprovement();
                peerSetCond.notify_all();
            }
            catch (const std::exception& ex) {
                peerStats.erase(rmtSockAddr);
                std::throw_with_nested(RUNTIME_ERROR("Couldn't add peer " +
                        rmtSockAddr.to_string()));
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

                return errNum != ECONNREFUSED &&
                       errNum != ECONNRESET &&
                       errNum != ENETUNREACH &&
                       errNum != ENETRESET &&
                       errNum != ENETDOWN &&
                       errNum != EHOSTUNREACH;
            }
        }
        catch (const std::runtime_error& ex) {
            return false; // Simple EOF
        }
        catch (const std::exception& innerEx) {
            return isFatal(innerEx);
        }

        return true;
    }

    /**
     * Waits until conditions are ripe for connecting to a remote peer-server.
     *
     * @cancellationpoint
     */
    void waitToConnect()
    {
        Lock lock{peerSetMutex};

        while (peerSet.size() >= maxPeers) {
            LOG_DEBUG("peerSet.size(): %zu", peerSet.size());
            peerSetCond.wait(lock);
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
        LOG_DEBUG("Improving P2P network");
        try {
            Lock lock{peerSetMutex};

            for (;;) {
                static auto timeout = std::chrono::seconds{timePeriod};

                for (auto time = improveStart + timeout;
                        peerSet.size() < maxPeers || Clock::now() < time;
                        time = improveStart + timeout)
                    peerSetCond.wait_until(lock, time); // Cancellation point

                peerStats.stopWorstPeer(); // Not a cancellation point
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
        LOG_DEBUG("Connecting to peers");
        try {
            for (;;) {
                waitToConnect(); // Cancellation point

                SockAddr srvrAddr;
                try {
                    // Cancellation point
                    srvrAddr = serverPool.pop(); // May block
                }
                catch (const std::exception& ex) {
                    LOG_DEBUG("Caught std::exception");
                    throw;
                }
                catch (...) {
                    LOG_DEBUG("Caught ... exception");
                    throw;
                }

                try {
                    LOG_DEBUG("Connecting to %s", srvrAddr.to_string().c_str());
                    // Cancellation point
                    Peer peer = factory.connect(srvrAddr); // Potentially slow

                    {
                        Canceler canceler{false};
                        // Won't add if peer-set is full
                        if (!add(peer, true))
                            serverPool.consider(srvrAddr, timePeriod);
                    }
                }
                catch (const std::exception& ex) {
                    serverPool.consider(srvrAddr, timePeriod);

                    if (isFatal(ex))
                        throw ex;

                    log_note(ex);
                }
            }
        }
        catch (const std::exception& ex) {
            LOG_DEBUG("Caught std::exception: %s", ex.what());
            setException(std::make_exception_ptr(ex));
        }
        catch (...) {
            LOG_DEBUG("Caught ... exception");
            throw;
        }
    }

    /**
     * Accepts incoming connections from remote peers and attempts to add the
     * resulting local peer to the set of active peers. Executes on a new
     * thread.
     *
     * @cancellationpoint
     */
    void accept()
    {
        LOG_DEBUG("Accepting peers");
        try {
            for (;;) {
                LOG_DEBUG("Accepting connection");
                Peer peer = factory.accept(); // Potentially slow
                LOG_DEBUG("Accepted peer %s",
                        peer.getRmtAddr().to_string().c_str());

                (void)add(peer); // Won't add if peer-set is full
            }
        }
        catch (const std::exception& ex) {
            LOG_DEBUG(ex, "Caught std::exception");
            setException(std::make_exception_ptr(ex));
        }
    }

    /**
     * Starts the tasks of this instance on new threads.
     */
    void startTasks()
    {
        if (maxPeers > 1)
            LOG_DEBUG("Creating \"improvement\" thread");
            improveThread = std::thread(&Impl::improve, this);

        try {
            LOG_DEBUG("Creating \"connect\" thread");
            connectThread = std::thread(&Impl::connect, this);

            try {
                LOG_DEBUG("Creating \"accept\" thread");
                acceptThread = std::thread(&Impl::accept, this);
            }
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
        }
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
     * Stops the tasks of this instance.
     */
    void stopTasks()
    {
        try {
            factory.close(); // Causes `factory.accept()` to return
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Error closing peer-factory"));
        }

        int status = ::pthread_cancel(connectThread.native_handle());
        if (status)
            throw SYSTEM_ERROR("Couldn't cancel \"connect\" thread", status);

        if (improveThread.joinable()) {
            status = ::pthread_cancel(improveThread.native_handle());
            if (status)
                throw SYSTEM_ERROR("Couldn't cancel \"improve\" thread", status);
        }

        acceptThread.join();
        connectThread.join();
        if (improveThread.joinable())
            improveThread.join();
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
     * Constructs.
     *
     * @param[in] srvrAddr     Socket address of local server that accepts
     *                         connections from remote peers
     * @param[in] listenSize   Size of server's `::listen()` queue
     * @param[in] portPool     Pool of available port numbers
     * @param[in] maxPeers     Maximum number of peers
     * @param[in] serverPool   Pool of possible remote servers for remote peers
     * @param[in] msgRcvr      Receiver of messages from peers
     * @param[in] timePeriod   Amount of time in seconds for the improvement
     *                         period and also the minimum amount of time before
     *                         the peer-server associated with a failed remote
     *                         peer is re-connected to
     */
    Impl(   const SockAddr& srvrAddr,
            const int       listenSize,
            PortPool&       portPool,
            const int       maxPeers,
            ServerPool&     serverPool,
            PeerMsgRcvr&    msgRcvr,
            const unsigned  timePeriod)
        : executing{ATOMIC_FLAG_INIT}
        , peerSetMutex{}
        , doneMutex{}
        , doneCond{}
        , peerSetCond{}
        , haltRequested{false}
        , timePeriod{timePeriod}
        , factory{srvrAddr, listenSize, portPool, *this}
        , maxPeers{maxPeers}
        , msgRcvr(msgRcvr)
        , taskException{}
        , serverPool{serverPool}
        , peerStats{}    // Must be destroyed after `peerSet`
        , peerSet{*this} // Must be destroyed before `peerStats`
        , improveStart{Clock::now()}
        , improveThread{}
        , connectThread{}
        , acceptThread{}
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
        if (executing.test_and_set())
            throw LOGIC_ERROR("Already called");

        startTasks();
        waitUntilDone();

        try {
            stopTasks();
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't stop tasks"));
        }

        Guard guard(doneMutex);

        LOG_DEBUG("haltRequested: %d", haltRequested);
        LOG_DEBUG("taskException: %sempty", taskException ? "not " : "");
        if (!haltRequested && taskException)
            std::rethrow_exception(taskException);
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

        Guard guard{peerSetMutex};

        // Possibly add the remote peer-server to the pool of remote servers
        bool fromConnect;
        try {
            fromConnect = peerStats.fromConnect(rmtSockAddr);
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(LOGIC_ERROR("Peer " + rmtSockAddr.to_string()
                    + " not found in performance map"));
        }

        if (fromConnect)
            serverPool.consider(rmtSockAddr, timePeriod);

        peerStats.erase(rmtSockAddr); // Remove performance entry
        resetImprovement();           // Restart performance evaluation
        peerSetCond.notify_all();
    }

    /**
     * Notifies all remote peers of an available chunk of data.
     *
     * @param[in] chunkId  Id of available chunk
     * @return    `true`   The notice was successfully enqueued to all peers
     * @return    `true`   The notice was not successfully enqueued to at least
     *                     one peer
     */
    bool notify(const ChunkId& chunkId)
    {
        return peerSet.notify(chunkId);
    }

    /**
     * Indicates if a chunk should be requested from a peer.
     *
     * @param[in] chunkId  ID of `Chunk`
     * @param[in] rmtAddr  Socket address of the remote peer
     * @retval    `true`   The chunk should be requested from the peer
     * @retval    `false`  The chunk should not be requested from the peer
     */
    bool shouldRequest(
            const ChunkId&  chunkId,
            const SockAddr& rmtAddr)
    {
        return msgRcvr.shouldRequest(chunkId, rmtAddr);
    }

    /**
     * Obtains a chunk for a peer.
     *
     * @param[in] chunkId  ID of requested `Chunk`
     * @param[in] rmtAddr  Socket address of the remote peer
     * @return             The chunk. Will be empty if it doesn't exist.
     */
    MemChunk get(
            const ChunkId&  chunkId,
            const SockAddr& rmtAddr)
    {
        return msgRcvr.get(chunkId, rmtAddr);
    }

    /**
     * Processes a `Chunk` from a peer.
     *
     * @param[in] chunk    The `Chunk`
     * @param[in] rmtAddr  Socket address of the remote peer
     */
    void hereIs(
            TcpChunk       chunk,
            const SockAddr& rmtAddr)
    {
        msgRcvr.hereIs(chunk, rmtAddr);
        peerStats.inc(rmtAddr);
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
        PeerMsgRcvr&    msgRcvr,
        const unsigned  timePeriod)
    : pImpl{new Impl(srvrAddr, listenSize, portPool, maxPeers, serverPool,
            msgRcvr, timePeriod)}
{}

void P2pMgr::operator ()()
{
    pImpl->operator()();
}

size_t P2pMgr::size() const
{
    return pImpl->size();
}

bool P2pMgr::notify(const ChunkId& chunkId)
{
    return pImpl->notify(chunkId);
}

void P2pMgr::halt() const
{
    pImpl->halt();
}

} // namespace
