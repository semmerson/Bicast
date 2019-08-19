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

#include <chrono>
#include <condition_variable>
#include <cstring>
#include <exception>
#include <mutex>
#include <thread>
#include <unistd.h>

namespace hycast {

class P2pMgr::Impl : public PeerMsgRcvr, public PeerSet::Observer
{
    typedef std::mutex              Mutex;
    typedef std::lock_guard<Mutex>  Guard;
    typedef std::unique_lock<Mutex> Lock;
    typedef std::condition_variable Cond;
    typedef std::exception_ptr      ExceptPtr;

    PeerSet        peerSet;
    const int      maxPeers;
    PeerFactory    factory;
    ServerPool     serverPool;
    mutable Mutex  doneMutex;
    mutable Mutex  peerSetMutex;
    mutable Cond   doneCond;
    mutable Cond   peerSetCond;
    ExceptPtr      taskException;
    bool           haltRequested;
    PeerMsgRcvr&   msgRcvr;
    std::thread    connectThread;
    std::thread    acceptThread;

    void handleTaskException()
    {
        Guard guard{doneMutex};

        if (!taskException) {
            taskException = std::current_exception(); // No throw
            doneCond.notify_one(); // No throw
        }
    }

    bool add(Peer& peer)
    {
        bool  success;

        {
            Guard guard{peerSetMutex};

            if (peerSet.size() >= maxPeers) {
                success = false;
            }
            else {
                (void)peerSet.activate(peer); // Fast
                success = true;
            }
        }

        if (!success)
            LOG_INFO("Didn't add peer %s because peer-set is full",
                    peer.to_string().c_str());

        return success;
    }

    bool fatal(const std::exception& ex)
    {
        try {
            std::rethrow_if_nested(ex);
        }
        catch (const std::system_error& sysEx) {
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
        catch (const std::exception& innerEx) {
            return fatal(innerEx);
        }

        return true;
    }

    void waitWhileFull()
    {
        Lock lock{peerSetMutex};

        while (peerSet.size() >= maxPeers)
            peerSetCond.wait(lock);
    }

    /**
     * Executes on a new thread.
     */
    void connect()
    {
        try {
            for (;;) {
                auto srvrAddr = serverPool.pop(); // May block
                try {
                    waitWhileFull();

                    Peer peer = factory.connect(srvrAddr); // Potentially slow

                    if (!add(peer)) // Won't add if peer-set is full
                        serverPool.consider(srvrAddr, 60);
                }
                catch (const std::exception& ex) {
                    serverPool.consider(srvrAddr, 60);

                    if (fatal(ex))
                        throw;
                }
            }
        }
        catch (const std::exception& ex) {
            handleTaskException();
        }
    }

    /**
     * Executes on a new thread.
     */
    void accept()
    {
        try {
            for (;;) {
                Peer peer = factory.accept(); // Potentially slow
                (void)add(peer); // Won't add if peer-set is full
            }
        }
        catch (const std::exception& ex) {
            handleTaskException();
        }
    }

    void startTasks()
    {
        connectThread = std::thread(&Impl::connect, this);

        try {
            acceptThread = std::thread(&Impl::accept, this);
        }
        catch (const std::exception& ex) {
            if (connectThread.joinable()) {
                ::pthread_cancel(connectThread.native_handle());
                connectThread.join();
            }
            throw;
        }
    }

    void stopTasks()
    {
        int status;

        status = ::pthread_cancel(acceptThread.native_handle());
        if (status)
            LOG_ERROR("Couldn't stop accept-thread: %s", ::strerror(status));

        status = ::pthread_cancel(connectThread.native_handle());
        if (status)
            LOG_ERROR("Couldn't stop connect-thread: %s", ::strerror(status));

        acceptThread.join();
        connectThread.join();
    }

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
     */
    Impl(   const SockAddr& srvrAddr,
            const int       listenSize,
            PortPool&       portPool,
            const int       maxPeers,
            ServerPool&     serverPool,
            PeerMsgRcvr&    msgRcvr)
        : peerSet{*this}
        , maxPeers{maxPeers}
        , factory{srvrAddr, listenSize, portPool, *this}
        , serverPool{serverPool}
        , doneMutex()
        , peerSetMutex()
        , doneCond()
        , peerSetCond()
        , taskException()
        , haltRequested{false}
        , msgRcvr(msgRcvr)
        , connectThread()
        , acceptThread()
    {}

    ~Impl() noexcept
    {
        Guard guard(doneMutex);

        if (acceptThread.joinable())
            throw RUNTIME_ERROR("Accept-thread is joinable");

        if (connectThread.joinable())
            throw RUNTIME_ERROR("Connect-thread is joinable");
    }

    /**
     * Executes this instance. Returns if
     *   - `halt()` is called
     *   - An exception is thrown
     * Returns immediately if `halt()` was called before this method.
     *
     * @threadsafety     Safe
     * @exceptionsafety  Basic guarantee
     */
    void operator ()()
    {
        startTasks();
        waitUntilDone();
        stopTasks();

        Guard guard(doneMutex);

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
        Guard guard(doneMutex);

        haltRequested = true;
        doneCond.notify_one();
    }

    size_t size() const
    {
        return peerSet.size();
    }

    bool notify(const ChunkId& chunkId)
    {
        return peerSet.notify(chunkId);
    }

    /**
     * Indicates if a chunk should be requested from a peer.
     *
     * @param[in] chunkId  ID of `Chunk`
     * @param[in] peer     Associated peer
     * @retval    `true`   The chunk should be requested from the peer
     * @retval    `false`  The chunk should not be requested from the peer
     */
    bool shouldRequest(
            const ChunkId& chunkId,
            Peer&          peer)
    {
        return msgRcvr.shouldRequest(chunkId, peer);
    }

    /**
     * Obtains a chunk for a peer.
     *
     * @param[in] chunkId  ID of requested `Chunk`
     * @param[in] peer     Associated peer
     * @return             The chunk. Will be empty if it doesn't exist.
     */
    MemChunk get(
            const ChunkId& chunkId,
            Peer&          peer)
    {
        return msgRcvr.get(chunkId, peer);
    }

    /**
     * Processes a `Chunk` from a peer.
     *
     * @param[in] chunk  The `Chunk`
     * @param[in] peer   Associated peer
     */
    void hereIs(
            WireChunk& chunk,
            Peer&      peer)
    {
        return msgRcvr.hereIs(chunk, peer);
    }

    /**
     * Handles a stopped peer. Called by the peer-set.
     *
     * @param[in] peer  The peer that stopped
     */
    void stopped(Peer& peer)
    {
        Guard guard(peerSetMutex);
        peerSetCond.notify_all();
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
        PeerMsgRcvr&    msgRcvr)
    : pImpl{new Impl(srvrAddr, listenSize, portPool, maxPeers, serverPool,
            msgRcvr)}
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
