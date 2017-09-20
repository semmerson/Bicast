/**
 * This file implements a manager of peer-to-peer connections, which is
 * responsible for processing incoming connection requests, creating peers, and
 * replacing peers.
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
#include "Future.h"
#include "InetSockAddr.h"
#include "MsgRcvr.h"
#include "Notifier.h"
#include "P2pMgr.h"
#include "PeerMsgRcvr.h"
#include "PeerSet.h"
#include "PeerSource.h"
#include "SctpSock.h"
#include "Thread.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <pthread.h>
#include <queue>
#include <set>
#include <thread>

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
class P2pMgr::Impl final : public Notifier
{
    typedef std::mutex              Mutex;
    typedef std::lock_guard<Mutex>  LockGuard;
    typedef std::unique_lock<Mutex> UniqueLock;
    typedef PeerSet::TimeUnit       TimeUnit;
    typedef PeerSet::Clock          Clock;

    class NilMsgRcvr : public PeerMsgRcvr
    {
    public:
        void recvNotice(const ProdInfo& info, const Peer& peer) {}
        void recvNotice(const ChunkInfo& info, const Peer& peer) {}
        void recvRequest(const ProdIndex& index, const Peer& peer) {}
        void recvRequest(const ChunkInfo& info, const Peer& peer) {}
        void recvData(LatentChunk chunk, const Peer& peer) {}
    };

    static NilMsgRcvr         nilMsgRcvr;

    /// Source of potential remote peers
    PeerSource&               peerSource;

    /// Internet address of local peer-server
    InetSockAddr              serverSockAddr;

    /// Object to receive incoming messages
    PeerMsgRcvr*              msgRcvr;

    /// Set of active peers
    PeerSet                   peerSet;

    /// Asynchronous task completion service
    Completer<void>           completer;

    /// Concurrent access variables for fatal exceptions:
    mutable Mutex                   exceptMutex;
    mutable std::condition_variable exceptCond;

    /// Concurrent access variables for the peer-set:
    Mutex                     peerSetMutex;
    std::condition_variable   peerSetCond;

    /// Maximum number of peers:
    unsigned                  maxPeers;

    /// Duration to wait before trying to replace the worst-performing peer.
    TimeUnit                  stasisDuration;

    /// Remote socket address of initiated peers
    std::set<InetSockAddr>    initiatedPeers;

    /// Is `msgRcvr` set?
    bool                      msgRcvrSet;

    /// Has `run()` been called?
    bool                      isRunning;

    /// Exception that caused failure:
    std::exception_ptr        exception;

    /// Thread on which local peers are initiated:
    Thread                    peerAddrThread;

    /// Thread on which remote peers are accepted:
    Thread                    serverThread;

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
                SrvrSctpSock serverSock{serverSockAddr, Peer::getNumStreams()};
                serverSock.listen();
                for (;;) {
                    auto sock = serverSock.accept(); // Blocks
                    LockGuard lock(peerSetMutex);
                    Peer peer;
                    try {
                        peer = Peer(*msgRcvr, sock);
                        if (peerSet.tryInsert(peer)) // Might block
                            LOG_NOTE("Accepted connection from remote peer %s",
                                    sock.to_string().c_str());
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
     * @param[in,out] msgRcvr    Receiver of messages from the remote peer
     * @return `true`            Peer was added
     * @return `false`           Peer wasn't added because it's already a member
     * @throw RuntimeError       Couldn't add peer
     * @exceptionsafety          Strong guarantee
     * @threadsafety             Safe
     */
    bool tryInsert(
            const InetSockAddr& peerAddr,
            PeerMsgRcvr&        msgRcvr)
    {
        bool success;
    	try {
            // Blocks until connected and versions exchanged
            Peer peer(msgRcvr, peerAddr);
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
                    if (tryInsert(peerAddr, *msgRcvr)) { // Blocks
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
     * Constructs.
     * @param[in]     serverSockAddr  Socket address to be used by the server
     *                                that remote peers connect to
     * @param[in]     maxPeers        Maximum number of active peers
     * @param[in]     peerSource      Source of potential remote peers
     * @param[in]     stasisDuration  Time interval over which the set of active
     *                                peers must be unchanged before the worst
     *                                performing peer may be replaced
     * @param[in,out] msgRcvr         Receiver of messages from remote peers
     */
    Impl(   const InetSockAddr&       serverSockAddr,
            const unsigned            maxPeers,
            PeerSource&               peerSource,
            const PeerSet::TimeUnit   stasisDuration,
            PeerMsgRcvr&              msgRcvr)
        : peerSource(peerSource)
        , serverSockAddr{serverSockAddr}
        , msgRcvr(&msgRcvr)
        , peerSet{stasisDuration*2, maxPeers,
                [this](const InetSockAddr& peerAddr) {
                    handleStoppedPeer(peerAddr);
                }}
        , completer{}
        , exceptMutex{}
        , exceptCond{}
        , peerSetMutex{}
        , peerSetCond{}
        , maxPeers{maxPeers}
        , stasisDuration{stasisDuration}
        , msgRcvrSet{true}
        , isRunning{false}
        , exception{}
        , peerAddrThread{}
        , serverThread{}
    {}

    ~Impl()
    {
        stopThreads(this);
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
        isRunning = true;
        while (!exception) {
            Canceler canceler{};
            exceptCond.wait(lock);
        }
        std::rethrow_exception(exception);
        THREAD_CLEANUP_POP(true);
    }

    /**
     * Sends information about a product to the remote peers.
     * @param[in] prodInfo        Product information
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void sendNotice(const ProdInfo& prodInfo) const
    {
        checkException();
        peerSet.sendNotice(prodInfo);
    }

    /**
     * Sends information about a product to the remote peers except for one.
     * @param[in] prodInfo        Product information
     * @param[in] except          Peer to exclude
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void sendNotice(const ProdInfo& prodInfo, const Peer& except) const
    {
        checkException();
        peerSet.sendNotice(prodInfo, except.getRemoteAddr());
    }

    /**
     * Sends information about a chunk-of-data to the remote peers.
     * @param[in] chunkInfo       Chunk information
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void sendNotice(const ChunkInfo& chunkInfo) const
    {
        checkException();
        peerSet.sendNotice(chunkInfo);
    }

    /**
     * Sends information about a chunk-of-data to the remote peers except for
     * one.
     * @param[in] chunkInfo       Chunk information
     * @param[in] except          Peer to exclude
     * @throws std::system_error  I/O error occurred
     * @exceptionsafety           Basic
     * @threadsafety              Compatible but not safe
     */
    void sendNotice(const ChunkInfo& chunkInfo, const Peer& except) const
    {
        checkException();
        peerSet.sendNotice(chunkInfo, except.getRemoteAddr());
    }
};

P2pMgr::Impl::NilMsgRcvr P2pMgr::Impl::nilMsgRcvr;

P2pMgr::P2pMgr(
        const InetSockAddr&      serverSockAddr,
        PeerMsgRcvr&             msgRcvr,
        PeerSource&              peerSource,
        const unsigned           maxPeers,
        const PeerSet::TimeUnit  stasisDuration)
    : pImpl{new Impl(serverSockAddr, maxPeers, peerSource, stasisDuration,
            msgRcvr)}
{}

void P2pMgr::run()
{
    pImpl->run();
}

void P2pMgr::sendNotice(const ProdInfo& prodInfo) const
{
    pImpl->sendNotice(prodInfo);
}

void P2pMgr::sendNotice(const ProdInfo& prodInfo, const Peer& except) const
{
    pImpl->sendNotice(prodInfo, except);
}

void P2pMgr::sendNotice(const ChunkInfo& chunkInfo) const
{
    pImpl->sendNotice(chunkInfo);
}

void P2pMgr::sendNotice(const ChunkInfo& chunkInfo, const Peer& except) const
{
    pImpl->sendNotice(chunkInfo, except);
}

} // namespace
