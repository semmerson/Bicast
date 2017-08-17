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
#include "DelayQueue.h"
#include "error.h"
#include "Future.h"
#include "InetSockAddr.h"
#include "MsgRcvr.h"
#include "Notifier.h"
#include "P2pMgr.h"
#include "PeerMsgRcvr.h"
#include "PeerSet.h"
#include "PeerSource.h"
#include "SrvrSctpSock.h"
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

    /// Delay-queue for initiated peers
    DelayQueue<InetSockAddr>  peerAddrs;

    /// Internet address of peer-server
    InetSockAddr              serverSockAddr;

    /// Object to receive incoming messages
    PeerMsgRcvr*              msgRcvr;

    /// Set of active peers
    PeerSet                   peerSet;

    /// Asynchronous task completion service
    Completer<void>           completer;

    /// Concurrent access variables for fatal exceptions:
    Mutex                     exceptMutex;
    std::condition_variable   exceptCond;

    /// Concurrent access variables for the peer-set:
    Mutex                     peerSetMutex;
    std::condition_variable   peerSetCond;

    /// Maximum number of peers:
    unsigned                  maxPeers;

    /// Duration to wait before trying to replace the worst-performing peer.
    TimeUnit                  stasisDuration;

    /// Remote socket address of initiated peers
    std::set<InetSockAddr>    initiated;

    /// Is `msgRcvr` set?
    bool                      msgRcvrSet;

    /// Has `operator()()` been called?
    bool                      isRunning;

    /// Exception that caused failure:
    std::exception_ptr        exception;

    /// Thread on which local peers are initiated:
    Thread                    peerAddrThread;

    /// Thread on which remote peers are accepted:
    Thread                    serverThread;

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
                for (;;) {
                    auto sock = serverSock.accept(); // Blocks
                    LockGuard lock(peerSetMutex);
                    Peer peer;
                    try {
                        peer = Peer(*msgRcvr, sock);
                        peerSet.tryInsert(peer); // Might block
                    }
                    catch (const std::exception& e) {
                        log_what(e); // Possibly incompatible protocol versions
                    }
                }
            }
            catch (const std::exception& e) {
                std::throw_with_nested(RuntimeError(__FILE__, __LINE__,
                        "Server for remote peers failed"));
            }
        }
        catch (const std::exception& e) {
            exception = std::current_exception();
            exceptCond.notify_one();
        }
    }

    /**
     * Tries to insert a remote peer given its Internet socket address. The peer
     * will be an *initiated* peer (i.e., one that was initiated by this
     * instance).
     * @param[in]     peerAddr   Socket address of remote peer candidate
     * @param[in,out] msgRcvr    Receiver of messages from the remote peer
     * @return `true`            Peer was added
     * @return `false`           Peer wasn't added because it's already a member
     * @throw LogicError         Unknown protocol version from remote peer. Peer
     *                           not added to set.
     * @throw RuntimeError       Other error
     * @exceptionsafety          Strong guarantee
     * @threadsafety             Safe
     */
    bool tryInsert(
            const InetSockAddr& peerAddr,
            PeerMsgRcvr&        msgRcvr)
    {
        bool success;
    	try {
            bool enabled = Thread::enableCancel();
            Peer peer(msgRcvr, peerAddr); // Blocks
            Thread::enableCancel(enabled);
            success = peerSet.tryInsert(peer); // Might block
        }
        catch (const LogicError& e) {
            // Unknown protocol version from remote peer
            throw;
        }
        catch (const std::exception& e) {
            std::throw_with_nested(RuntimeError(__FILE__, __LINE__,
                    "Couldn't add remote peer " + peerAddr.to_string() +
                    " to set of active peers"));
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
            try {
                for (;;) {
                    Thread::enableCancel();
                    auto peerAddr = peerAddrs.pop(); // Blocks
                    Thread::disableCancel();
                    try {
                        if (tryInsert(peerAddr, *msgRcvr)) { // Blocks
                            // Peer wasn't a member of active set
                            initiated.insert(peerAddr);
                        }
                    }
                    catch (const LogicError& e) {
                        // Unknown protocol from remote peer
                        log_what(e);
                        // Try again later
                        peerAddrs.push(peerAddr, stasisDuration);
                    }
                }
            }
            catch (const std::exception& e) {
                std::throw_with_nested(RuntimeError(__FILE__, __LINE__,
                        "Peer-adder failed"));
            }
        }
        catch (const std::exception& e) {
            log_what(e); // Because end of thread
        	exception = std::current_exception();
        	exceptCond.notify_one();
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
        LockGuard lock(peerSetMutex);
        initiated.erase(peerAddr);
        peerAddrs.push(peerAddr, stasisDuration);
        peerSetCond.notify_one();
    }

    /**
     * Cancels the threads.
     */
    static void stopThreads(void* arg)
    {
    	auto pImpl = reinterpret_cast<Impl*>(arg);
        pImpl->serverThread.cancel();
        pImpl->peerAddrThread.cancel();
    }

public:
    /**
     * Constructs.
     * @param[in]     serverSockAddr  Socket address to be used by the server
     *                                that remote peers connect to
     * @param[in]     maxPeers        Maximum number of active peers
     * @param[in]     peerAddrs       Potential remote peers to contact or
     *                                `nullptr`, in which case no remote peers
     *                                are contacted
     * @param[in]     stasisDuration  Time interval over which the set of active
     *                                peers must be unchanged before the worst
     *                                performing peer may be replaced
     * @param[in,out] msgRcvr         Receiver of messages from remote peers
     */
    Impl(   const InetSockAddr&       serverSockAddr,
            const unsigned            maxPeers,
            DelayQueue<InetSockAddr>  peerAddrs,
            const PeerSet::TimeUnit   stasisDuration,
            PeerMsgRcvr&              msgRcvr)
        : peerAddrs{peerAddrs}
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

    /**
     * Runs this instance. Starts receiving connection requests from remote
     * peers. Adds peers to the set of active peers. Replaces the worst
     * performing peer when appropriate. Doesn't return unless an exception is
     * thrown. Intended to run on its own thread, which may be safely cancelled.
     * @exceptionsafety Basic guarantee
     * @threadsafety    Compatible but not safe
     */
    void operator()()
    {
        UniqueLock lock(exceptMutex);
        if (peerAddrs.size())
            peerAddrThread = Thread{[this]{runPeerAdder();}};
        try {
            THREAD_CLEANUP_PUSH(stopThreads, this);
            serverThread = Thread{[this]{runServer();}};
            isRunning = true;
            Thread::enableCancel();
            while (!exception)
                exceptCond.wait(lock);
            Thread::disableCancel();
            THREAD_CLEANUP_POP(1);
            std::rethrow_exception(exception);
        }
        catch (const std::exception& e) {
            stopThreads(this);
            throw;
        }
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
    	if (exception)
            std::rethrow_exception(exception);
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
    	if (exception)
            std::rethrow_exception(exception);
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
    	if (exception)
            std::rethrow_exception(exception);
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
    	if (exception)
            std::rethrow_exception(exception);
        peerSet.sendNotice(chunkInfo, except.getRemoteAddr());
    }
};

P2pMgr::Impl::NilMsgRcvr P2pMgr::Impl::nilMsgRcvr;

P2pMgr::P2pMgr(
        const InetSockAddr&      serverSockAddr,
        PeerMsgRcvr&             msgRcvr,
        const unsigned           maxPeers,
        DelayQueue<InetSockAddr> peerAddrs,
        const PeerSet::TimeUnit  stasisDuration)
    : pImpl{new Impl(serverSockAddr, maxPeers, peerAddrs, stasisDuration,
            msgRcvr)}
{}

void P2pMgr::operator()()
{
    pImpl->operator()();
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
