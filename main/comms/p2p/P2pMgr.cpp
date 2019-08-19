/**
 * This file implements a manager of peer-to-peer connections. It runs the
 * server for incoming connections from remote peers, creates local peers from
 * a source of peers, and manages the set of active peers.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: P2pMgr.cpp
 * @author: Steven R. Emmerson
 */
#include "config.h"

#include "Completer.h"
#include "error.h"
#include "PeerSetServer.h"
#include "P2pMgr.h"
#include "P2pMgrServer.h"
#include "PeerSet.h"
#include "PeerSource.h"
#include "Thread.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <set>

namespace hycast {

/**
 * Class that implements a manager of peer-to-peer (P2P) connections.
 *
 * For reliability of data delivery, the following (overly cautious but simple)
 * protocol is implemented in order to ensure that every node in the P2P overlay
 * network has a path through the network from it to the source of the
 * data-products:
 *
 *   - An initiated peer is one that this class creates from a connection that
 *     it initiates (as opposed to a connection that it accepts).
 *   - This class accepts connections only if it has at least one initiated peer.
 *   - This class closes all connections when it has no initiated peers.
 */
class P2pMgr::Impl final : public Notifier, public PeerSetServer
{
    typedef std::mutex              Mutex;
    typedef std::lock_guard<Mutex>  LockGuard;
    typedef std::unique_lock<Mutex> UniqueLock;
    typedef std::condition_variable Cond;

    /// Source of potential locally-initiated peers
    PeerSource&               peerSource;

    /// Internet address of local peer-server
    InetSockAddr              serverSockAddr;

    /// Higher-level component used by a manager of peers.
    P2pMgrServer&             p2pMgrServer;

    /// Set of active peers
    PeerSet                   peerSet;

    /// Asynchronous task completion service
    Completer<void>           completer;

    /// Concurrent access variables for fatal exceptions:
    mutable Mutex             exceptMutex;
    mutable Cond              exceptCond;

    /// Concurrent access variables for the set of active peers:
    Mutex                     peerSetMutex;
    Cond                      peerSetCond;

    /// Maximum number of peers:
    unsigned                  maxPeers;

    /// Duration to wait before trying to replace the worst-performing peer.
    unsigned                  stasisDuration;

    /// Remote socket address of initiated peers
    std::set<InetSockAddr>    initiatedPeers;

    /// Exception that caused failure:
    std::exception_ptr        exception;

    /// Thread on which local peers are initiated:
    Thread                    peerAddrThread;

    /// Thread on which remote peers are accepted:
    Thread                    serverThread;

    /// Socket for server that listens for connections from remote peers
    SrvrSctpSock              serverSock;

    void setException()
    {
        LockGuard lock{exceptMutex};
        exception = std::current_exception();
        exceptCond.notify_one();
    }

    void checkException() const
    {
        LockGuard lock{exceptMutex};
        if (exception)
            std::rethrow_exception(exception);
    }

    /**
     * Runs the server for connections from remote peers. Creates a
     * corresponding local peer and attempts to add it to the set of active
     * peers if and only if at least one initiated peer exists. Doesn't return
     * unless an exception is thrown. Intended to run on its own thread, which
     * can be safely canceled.
     * @exceptionsafety Basic guarantee
     * @threadsafety    Compatible but not safe
     */
    void runServer()
    {
        try {
            try {
                SrvrSctpSock serverSock{serverSockAddr, PeerMsgSndr::getNumStreams()};
                serverSock.listen();
                for (;;) {
                    auto sock = serverSock.accept(); // Blocks
                    LockGuard lock(peerSetMutex);
                    try {
                        PeerMsgSndr peer{sock};
                        if (peerSet.tryInsert(peer)) // Might block
                            LOG_NOTE("Accepted connection from remote peer " +
                                    peer.getRemoteAddr().to_string());
                    }
                    catch (const std::exception& e) {
                        log_warn(e); // Possibly incompatible protocol versions
                    }
                }
            }
            catch (const std::exception& e) {
                std::throw_with_nested(RUNTIME_ERROR(
                        "Server for remote peers failed"));
            }
        }
        catch (const std::exception& e) {
            setException();
        }
    }

    /**
     * Tries to insert a remote peer given its Internet socket address. The peer
     * will be an *initiated* peer (i.e., one whose connection was initiated by
     * this instance).
     * @param[in]     peerAddr   Socket address of remote peer candidate
     * @return `true`            Peer was added
     * @return `false`           Peer wasn't added because it's already a member
     * @throw RuntimeError       Couldn't add peer
     * @exceptionsafety          Strong guarantee
     * @threadsafety             Safe
     */
    bool tryInsert(const InetSockAddr& peerAddr)
    {
        bool success;
    	try {
            // Blocks until connected and versions exchanged
            PeerMsgSndr peer{peerAddr};
            success = peerSet.tryInsert(peer); // Might block
        }
        catch (const std::exception& e) {
            std::throw_with_nested(RuntimeError{__FILE__, __LINE__,
                    "Couldn't add remote peer " + peerAddr.to_string() +
                    " to set of active peers"});
        }
        return success; // Eclipse wants to see return
    }

    /**
     * Attempts to add peers to the set of active peers. Doesn't return unless
     * an exception is thrown. Intended to run on its own thread, which can be
     * safely canceled.
     */
    void runPeerAdder()
    {
        try {
            for (;;) {
                auto peerAddr = peerSource.pop(); // Blocks. Cancellation point
                try {
                    if (tryInsert(peerAddr)) { // Blocks
                        LOG_NOTE("Initiated connection to remote peer " +
                                peerAddr.to_string());
                        // Peer wasn't a member of active set
                        initiatedPeers.insert(peerAddr);
                    }
                }
                catch (const std::exception& e) {
                    log_warn(e);
                    // Try again later
                    peerSource.push(peerAddr, stasisDuration);
                }
            }
        }
        catch (std::exception& e) {
            try {
                std::throw_with_nested(RUNTIME_ERROR("Peer-adder failed"));
            }
            catch (const std::exception& ex) {
                // Because end of thread
                log_error(ex);
                setException();
            }
        }
    }

    /***
     * Handles a stopped peer. Called by the active peer-set. Removes the peer
     * from the set of initiated peers, if applicable, adds the peer address to
     * the delay queue, and signals the adder of locally-initiated peers.
     * @param[in] peerAddr  Address of remote peer
     */
    void handleStoppedPeer(const InetSockAddr& peerAddr)
    {
        LOG_NOTE("Connection with remote peer terminated: peer=%s",
                peerAddr.to_string().c_str());
        LockGuard lock(peerSetMutex);
        initiatedPeers.erase(peerAddr);
        peerSource.push(peerAddr, stasisDuration);
        peerSetCond.notify_one();
    }

    /**
     * Cancels the threads.
     */
    static void stopThreads(void* arg)
    {
    	auto pImpl = static_cast<Impl*>(arg);

        pImpl->serverThread.cancel();
        pImpl->serverThread.join();

        pImpl->peerAddrThread.cancel();
        pImpl->peerAddrThread.join();
    }

public:
    /**
     * Constructs. Upon return, the server's socket is not listening and connections
     * won't be accepted until `run()` is called.
     * @param[in]     serverSockAddr  Socket address to be used by the server
     *                                that remote peers connect to
     * @param[in]     maxPeers        Maximum number of active peers
     * @param[in]     peerSource      Source of potential remote peers
     * @param[in]     stasisDuration  Time interval over which the set of active
     *                                peers must be unchanged before the worst
     *                                performing peer may be replaced
     * @param[in,out] peerSetServer   Higher-level component for the set of
     *                                active peers
     * @see `run()`
     */
    Impl(   const InetSockAddr&       serverSockAddr,
            const unsigned            maxPeers,
            PeerSource&               peerSource,
            const unsigned            stasisDuration,
            P2pMgrServer&             p2pMgrServer)
        : peerSource(peerSource)
        , serverSockAddr{serverSockAddr}
        , p2pMgrServer(p2pMgrServer)
        , peerSet{*this, maxPeers, stasisDuration*2}
        , completer{}
        , exceptMutex{}
        , exceptCond{}
        , peerSetMutex{}
        , peerSetCond{}
        , maxPeers{maxPeers}
        , stasisDuration{stasisDuration}
        , exception{}
        , peerAddrThread{}
        , serverThread{}
        , serverSock{serverSockAddr, PeerMsgSndr::getNumStreams()}
    {}

    ~Impl() noexcept
    {
        try {
            stopThreads(this);
        }
        catch (const std::exception& ex) {
            log_error(ex);
        }
    }

    /**
     * Runs this instance. Starts receiving connection requests from remote
     * peers. Adds peers to the set of active peers. Replaces the worst
     * performing peer when appropriate. Doesn't return unless an exception is
     * thrown. Intended to run on its own thread, which may be safely cancelled.
     * @exceptionsafety Basic guarantee
     * @threadsafety    Compatible but not safe
     */
    void run()
    {
        UniqueLock lock(exceptMutex);
        THREAD_CLEANUP_PUSH(stopThreads, this);
        if (!peerSource.empty())
            peerAddrThread = Thread{[this]{runPeerAdder();}};
        serverThread = Thread{[this]{runServer();}};
        while (!exception) {
            Canceler canceler{};
            exceptCond.wait(lock);
        }
        std::rethrow_exception(exception);
        THREAD_CLEANUP_POP(true);
    }

    /**
     * Notifies all active peers about available information on a product.
     * @param[in] prodIndex       Product index
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void notify(const ProdIndex& prodIndex) const
    {
        checkException();
        peerSet.notify(prodIndex);
    }

    /**
     * Notifies all active peers about an available chunk-of-data.
     * @param[in] chunkId         Chunk identifier
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void notify(const ChunkId& chunkId) const
    {
        checkException();
        peerSet.notify(chunkId);
    }

    // Begin implementation of `PeerSetServer`

    ChunkId getEarliestMissingChunkId() {
        return p2pMgrServer.getEarliestMissingChunkId();
    }

    Backlogger getBacklogger(
            const ChunkId& earliest,
            PeerMsgSndr&          peer)
    {
        return p2pMgrServer.getBacklogger(earliest, peer);
    }
    void peerStopped(const InetSockAddr& peerAddr)
    {
        handleStoppedPeer(peerAddr);
    }
    bool shouldRequest(const ProdIndex& index)
    {
        return p2pMgrServer.shouldRequest(index);
    }
    bool shouldRequest(const ChunkId& chunkId)
    {
        return p2pMgrServer.shouldRequest(chunkId);
    }
    bool get(const ProdIndex& index, ProdInfo& info)
    {
        return p2pMgrServer.get(index, info);
    }
    bool get(const ChunkId& chunkId, ActualChunk& chunk)
    {
        return p2pMgrServer.get(chunkId, chunk);
    }
    RecvStatus receive(
            const ProdInfo&     info,
            const InetSockAddr& peerAddr)
    {
        return p2pMgrServer.receive(info, peerAddr);
    }
    RecvStatus receive(
            LatentChunk&        chunk,
            const InetSockAddr& peerAddr)
    {
        return p2pMgrServer.receive(chunk, peerAddr);
    }
};

P2pMgr::P2pMgr(
        const InetSockAddr&      serverSockAddr,
        P2pMgrServer&            p2pMgrServer,
        PeerSource&              peerSource,
        const unsigned           maxPeers,
        const unsigned           stasisDuration)
    : pImpl{new Impl(serverSockAddr, maxPeers, peerSource, stasisDuration,
            p2pMgrServer)}
{}

void P2pMgr::run()
{
    pImpl->run();
}

void P2pMgr::notify(const ProdIndex& prodIndex) const
{
    pImpl->notify(prodIndex);
}

void P2pMgr::notify(const ChunkId& chunkId) const
{
    pImpl->notify(chunkId);
}

} // namespace
