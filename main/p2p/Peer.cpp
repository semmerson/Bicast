/**
 * A local peer that communicates with its associated remote peer. Besides
 * sending notices to the remote peer, this class also creates and runs
 * independent threads that receive messages from the remote peer and pass them
 * to a peer manager.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Peer.cpp
 *  Created on: May 29, 2019
 *      Author: Steven R. Emmerson
 */
#include "config.h"

#include "ChunkIdQueue.h"
#include "error.h"
#include "hycast.h"
#include "NodeType.h"
#include "Peer.h"

#include <cassert>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <pthread.h>
#include <PeerProto.h>
#include <thread>
#include <vector>

namespace hycast {

/**
 * Abstract base class for a peer implementation.
 */
class Peer::Impl : public SendPeer
{
    void runPeerProto()
    {
        try {
            peerProto();
            // The remote peer closed the connection
            halt();
        }
        catch (const std::exception& ex) {
            handleException(ex);
        }
    }

    void runNotifier()
    {
        try {
            for (;;) {
                auto chunkId = noticeQueue.pop();

                if (isDone())
                    break;

                chunkId.notify(peerProto);
            }
        }
        catch (const std::exception& ex) {
            LOG_DEBUG("Caught exception \"%s\"", ex.what());
            handleException(ex);
        }
        catch (...) {
            LOG_DEBUG("Caught exception ...");
            throw;
        }
    }

protected:
    using Thread = std::thread;
    using Mutex = std::mutex;
    using Guard = std::lock_guard<Mutex>;
    using Lock = std::unique_lock<Mutex>;
    using Cond = std::condition_variable;
    using AtomicBool = std::atomic<bool>;
    using ExceptPtr = std::exception_ptr;

    mutable Mutex  mutex;           ///< State-change mutex
    mutable Cond   cond;            ///< State-change condition variable
    SendPeerMgr&   peerMgr;         ///< Peer manager interface
    ChunkIdQueue   noticeQueue;     ///< Queue for notices
    Thread         notifierThread;  ///< Thread on which notices are sent
    Thread         protocolThread;  ///< Thread on which peerProto() executes
    ExceptPtr      exceptPtr;       ///< Pointer to terminating exception
    bool           done;            ///< Terminate without an exception?
    AtomicBool     isRunning;       ///< `operator()()` is active?
    PeerProto      peerProto;       ///< Peer-to-peer protocol object
    const SockAddr rmtAddr;         ///< Socket address of remote peer
    const SockAddr lclAddr;         ///< Socket address of local peer

    void handleException(const std::exception& ex)
    {
        Guard guard(mutex);

        if (!exceptPtr) {
            LOG_DEBUG("Setting exception");
            exceptPtr = std::make_exception_ptr(ex);
            cond.notify_all();
        }
    }

    bool isDone()
    {
        Guard guard{mutex};
        return done;
    }

    void ensureNotDone()
    {
        assert(!mutex.try_lock());
        if (done)
            throw LOGIC_ERROR("Peer has been halted");
    }

    void waitUntilDone()
    {
        Lock lock(mutex);

        while (!done && !exceptPtr)
            cond.wait(lock);
    }

    /**
     * @throw std::runtime_error  Couldn't create thread
     */
    void startNotifier()
    {
        try {
            notifierThread = std::thread{&Impl::runNotifier, this};
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(
                    RUNTIME_ERROR("Couldn't create notifier thread"));
        }
    }

    void stopNotifier() {
        if (notifierThread.joinable()) {
            noticeQueue.close();
            notifierThread.join();
        }
    }

    /**
     * @throw std::runtime_error  Couldn't create thread
     */
    void startProtocol()
    {
        try {
            protocolThread = std::thread{&Impl::runPeerProto, this};
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(
                    RUNTIME_ERROR("Couldn't create protocol thread"));
        }
    }

    void stopProtocol() {
        if (protocolThread.joinable()) {
            peerProto.halt();
            protocolThread.join();
        }
    }

    virtual void startTasks() =0;

    /**
     * Idempotent.
     */
    virtual void stopTasks() =0;

public:
    /**
     * Constructs. Applicable to both a publisher and a subscriber.
     *
     * @param[in] peerProto    Peer protocol
     * @param[in] peerMgr      Peer manager
     */
    Impl(   PeerProto&&  peerProto,
            SendPeerMgr& peerMgr)
        : mutex()
        , cond()
        , peerMgr(peerMgr)
        , noticeQueue{}
        , notifierThread{}
        , protocolThread{}
        , exceptPtr()
        , done{false}
        , isRunning{false}
        , peerProto(peerProto)
        , rmtAddr(peerProto.getRmtAddr())
        , lclAddr(peerProto.getLclAddr())
    {}

    /**
     * Screams bloody murder if called before `halt()`: calls
     * `std::terminate()`.
     */
    virtual ~Impl()
    {
        if (isRunning)
            throw LOGIC_ERROR("Peer is still executing!");
    }

    /**
     * Returns the socket address of the remote peer regardless of the state of
     * the connection.
     *
     * @return            Socket address of the remote peer.
     * @cancellationpoint No
     */
    SockAddr getRmtAddr() const noexcept {
        return rmtAddr; // NB: Independent of connection state
    }

    /**
     * Returns the local socket address regardless of the state of the
     * connection.
     *
     * @return            Local socket address
     * @cancellationpoint No
     */
    SockAddr getLclAddr() const noexcept {
        return lclAddr; // NB: Independent of connection state
    }

    /**
     * Executes this instance by starting subtasks. Doesn't return until an
     * exception is thrown by a subtask or `halt()` is called. Upon return,
     * all subtasks have terminated. If `halt()` is called before this method,
     * then this instance will return immediately and won't execute.
     *
     * @throw std::system_error   System error
     * @throw std::runtime_error  Couldn't create necessary thread
     * @throw std::runtime_error  Remote peer closed the connection
     * @throw std::logic_error    This method has already been called
     */
    void operator ()()
    {
        isRunning = true;

        try {
            startTasks();

            try {
                waitUntilDone();

                {
                    Guard guard{mutex};
                    if (!done && exceptPtr)
                        std::rethrow_exception(exceptPtr);
                }

                stopTasks(); // Idempotent
                isRunning = false;
                LOG_NOTE("Peer " + to_string() + " stopped");
            } // Tasks started
            catch (...) {
                stopTasks(); // Idempotent
                throw;
            }
        } // Tasks started
        catch (const std::exception& ex) {
            isRunning = false;
            Guard guard{mutex};
            if (done) {
                LOG_NOTE("Peer " + to_string() + " stopped");
            }
            else {
                std::throw_with_nested(RUNTIME_ERROR("Peer " + to_string() +
                        " failed"));
            }
        }
        catch (...) {
            isRunning = false;
            throw;
        }
    }

    /**
     * Halts execution. Does nothing if `operator()()` has not been called;
     * otherwise, causes `operator()()` to return and disconnects from the
     * remote peer. *Must* be called if `operator()()` is called. Idempotent.
     *
     * @cancellationpoint  No
     * @asyncsignalsafety  Unsafe
     */
    void halt() noexcept
    {
        Guard guard{mutex};
        done = true;
        cond.notify_all();
    }

    std::string to_string() const noexcept
    {
        return "{rmtAddr: " + rmtAddr.to_string() + ", lclAddr: " +
                lclAddr.to_string() + "}";
    }

    /**
     * Indicates if this instance resulted from a call to `::connect()`.
     *
     * @retval `false`  No
     * @retval `true`   Yes
     */
    virtual bool isFromConnect() const noexcept =0;

    void notify(const ProdIndex prodIndex)
    {
        Guard guard{mutex};

        ensureNotDone();
        LOG_DEBUG("Enqueuing product-index " + prodIndex.to_string());
        noticeQueue.push(prodIndex);
    }

    void notify(const SegId& segId)
    {
        Guard guard{mutex};

        ensureNotDone();
        LOG_DEBUG("Enqueuing segment-ID " + segId.to_string());
        noticeQueue.push(segId);
    }

    void sendMe(const ProdIndex prodIndex)
    {
        LOG_DEBUG("Accepting request for information on product " +
                prodIndex.to_string());

        int entryState;

        ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &entryState);
            auto prodInfo = peerMgr.getProdInfo(rmtAddr, prodIndex);
        ::pthread_setcancelstate(entryState, &entryState);

        if (prodInfo) {
            //LOG_DEBUG("Sending product-information %s",
                    //prodInfo.to_string().data());
            peerProto.send(prodInfo);
        }
    }

    void sendMe(const SegId& segId)
    {
        try {
            LOG_DEBUG("Accepting request for data-segment %s",
                    segId.to_string().data());

            //::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &entryState);
                int entryState;
                MemSeg memSeg = peerMgr.getMemSeg(rmtAddr, segId);
            //::pthread_setcancelstate(entryState, &entryState);

            if (memSeg) {
                //LOG_DEBUG("Sending data-segment %s", memSeg.to_string().data());
                peerProto.send(memSeg);
            }
        }
        catch (const std::exception& ex) {
            LOG_DEBUG("Caught exception \"%s\"", ex.what());
            throw;
        }
        catch (...) {
            LOG_DEBUG("Caught exception ...");
            throw;
        }
    }

    virtual bool isPathToPub() const noexcept =0;

    virtual void gotPath() const =0;

    virtual void lostPath() const =0;

    virtual void request(const ProdIndex prodIndex) =0;

    virtual void request(const SegId& segId) =0;
};

Peer::Peer(const Peer& peer)
    : pImpl(peer.pImpl) {
}

Peer::operator bool() const noexcept {
    return static_cast<bool>(pImpl);
}

SockAddr Peer::getRmtAddr() const noexcept {
    return pImpl->getRmtAddr();
}

SockAddr Peer::getLclAddr() const noexcept {
    return pImpl->getLclAddr();
}

size_t Peer::hash() const noexcept {
    return std::hash<Impl*>()(pImpl.get());
}

std::string Peer::to_string() const noexcept {
    return pImpl->to_string();
}

Peer& Peer::operator=(const Peer& rhs) {
    pImpl = rhs.pImpl;

    return *this;
}

bool Peer::operator==(const Peer& rhs) const noexcept {
    return pImpl.get() == rhs.pImpl.get();
}

bool Peer::operator<(const Peer& rhs) const noexcept {
    return pImpl.get() < rhs.pImpl.get();
}

void Peer::operator ()() const {
    pImpl->operator()();
}

void Peer::halt() const noexcept {
    if (pImpl)
        pImpl->halt();
}

bool Peer::isFromConnect() const noexcept {
    return pImpl->isFromConnect();
}

bool Peer::isPathToPub() const noexcept {
    return pImpl->isPathToPub();
}

void Peer::gotPath() const {
    pImpl->gotPath();
}

void Peer::lostPath() const {
    pImpl->lostPath();
}

void Peer::notify(const ProdIndex prodIndex) const {
    pImpl->notify(prodIndex);
}

void Peer::notify(const SegId& segId) const {
    pImpl->notify(segId);
}

void Peer::request(const ProdIndex prodId) const {
    pImpl->request(prodId);
}

void Peer::request(const SegId& segId) const {
    pImpl->request(segId);
}

/******************************************************************************/

/**
 * A publisher-peer implementation.
 */
class PubPeer final : public Peer::Impl
{
protected:
    /**
     * @throw std::runtime_error  Couldn't create thread
     */
    void startTasks() override
    {
        startNotifier();

        try {
            startProtocol();
        } // Notifier started
        catch (const std::exception& ex) {
            LOG_DEBUG("Caught \"%s\"", ex.what());
            stopNotifier();
            throw;
        }
        catch (...) {
            LOG_DEBUG("Caught ...");
            stopNotifier();
            throw;
        }
    }

    /**
     * Idempotent.
     */
    void stopTasks() override
    {
        stopProtocol();
        stopNotifier();
    }

public:
    /**
     * Constructs. Server-side construction only.
     *
     * @param[in] sock         TCP socket with remote peer
     * @param[in] peerMgr      Peer manager
     */
    PubPeer(TcpSock&     sock,
            SendPeerMgr& peerMgr)
        : Peer::Impl(PeerProto(sock, *this), peerMgr)
    {}

    /**
     * Indicates if this instance resulted from a call to `::connect()`.
     * Publisher-peers don't call `::connect()`.
     *
     * @return `false`  Always
     */
    bool isFromConnect() const noexcept {
        return false;
    }

    bool isPathToPub() const noexcept {
        throw LOGIC_ERROR("Invalid call");
    }

    void gotPath() const {
        throw LOGIC_ERROR("Invalid call");
    }

    void lostPath() const {
        throw LOGIC_ERROR("Invalid call");
    }

    void request(const ProdIndex prodIndex) {
        throw LOGIC_ERROR("Invalid call");
    }

    void request(const SegId& segId) {
        throw LOGIC_ERROR("Invalid call");
    }
};

Peer::Peer() =default;

Peer::Peer(
        TcpSock&     sock,
        SendPeerMgr& peerMgr)
    : pImpl(new PubPeer(sock, peerMgr)) {
}

/******************************************************************************/

/**
 * A subscriber-peer implementation.
 */
class SubPeer final : public Peer::Impl, public RecvPeer
{
private:
    const bool      fromConnect;     ///< Instance is result of `::connect()`?
    ChunkIdQueue    requestQueue;    ///< Queue for requests
    std::thread     requesterThread; ///< Thread on which requests are made
    AtomicBool      rmtHasPathToPub; ///< Remote node has path to publisher?
    XcvrPeerMgr&    recvPeerMgr;     ///< Manager of subscriber peer

    void runRequester(void)
    {
        try {
            for (;;) {
                auto chunkId = requestQueue.pop();

                if (isDone())
                    break;

                chunkId.request(peerProto);
            }
        }
        catch (const std::exception& ex) {
            handleException(ex);
        }
    }

    /**
     * @throw std::runtime_error  Couldn't create thread
     */
    void startRequester() {
        try {
            requesterThread = std::thread{&SubPeer::runRequester, this};
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(
                    RUNTIME_ERROR("Couldn't create requester thread"));
        }
    }

    void stopRequester() {
        if (requesterThread.joinable()) {
            requestQueue.close();
            requesterThread.join();
        }
    }

    /**
     * @throw std::runtime_error  Couldn't create necessary thread
     */
    void startTasks()
    {
        startNotifier();

        try {
            startRequester();

            try {
                startProtocol();
            } // Requester started
            catch (const std::exception& ex) {
                LOG_DEBUG("Caught \"%s\"", ex.what());
                stopRequester();
                throw;
            }
            catch (...) {
                LOG_DEBUG("Caught ...");
                stopRequester();
                throw;
            }
        } // Notifier started
        catch (const std::exception& ex) {
            LOG_DEBUG("Caught \"%s\"", ex.what());
            stopNotifier();
            throw;
        }
        catch (...) {
            LOG_DEBUG("Caught ...");
            stopNotifier();
            throw;
        }
    }

    /**
     * Idempotent.
     */
    void stopTasks()
    {
        stopProtocol();
        stopRequester();
        stopNotifier();
    }

public:
    /**
     * Server-side construction (i.e., from an `::accept()`).
     *
     * @param[in] sock         TCP socket with remote peer
     * @param[in] lclNodeType  Type of local node
     * @param[in] peerMgr      This instance's manager
     */
    SubPeer(TcpSock&        sock,
            const NodeType& lclNodeType,
            XcvrPeerMgr&    peerMgr)
        : Peer::Impl(PeerProto(sock, lclNodeType, *this), peerMgr)
        , fromConnect{false}
        , requestQueue{}
        , requesterThread{}
        , rmtHasPathToPub{peerProto.getRmtNodeType()}
        , recvPeerMgr(peerMgr)
    {}

    /**
     * Client-side construction (i.e., uses `::connect()`).
     *
     * @param[in] rmtSrvrAddr  Address of remote peer-server
     * @param[in] lclNodeType  Type of local node
     * @param[in] peerMgr      This instance's manager
     * @throws    LogicError   `lclNodeType == NodeType::PUBLISHER`
     */
    SubPeer(const SockAddr& rmtSrvrAddr,
            const NodeType  lclNodeType,
            XcvrPeerMgr&    peerMgr)
        : Peer::Impl(PeerProto(rmtSrvrAddr, lclNodeType, *this), peerMgr)
        , fromConnect{true}
        , requestQueue{}
        , requesterThread{}
        , rmtHasPathToPub{peerProto.getRmtNodeType()}
        , recvPeerMgr(peerMgr)
    {}

    SendPeer& asSendPeer() noexcept
    {
        return *this;
    }

    /**
     * Indicates if this instance resulted from a call to `::connect()`.
     *
     * @retval `false`  No
     * @retval `true`   Yes
     */
    bool isFromConnect() const noexcept {
        return fromConnect;
    }

    /**
     * Notifies the remote peer that this local node just transitioned to being
     * a path to the source of data-products.
     */
    void gotPath() const
    {
        if (peerProto.getRmtNodeType() != NodeType::PUBLISHER)
            peerProto.gotPath();
    }

    /**
     * Notifies the remote peer that this local node just transitioned to not
     * being a path to the source of data-products.
     */
    void lostPath() const
    {
        if (peerProto.getRmtNodeType() != NodeType::PUBLISHER)
            peerProto.lostPath();
    }

    void notify(ProdIndex prodIndex)
    {
        if (peerProto.getRmtNodeType() != NodeType::PUBLISHER) {
            Guard guard{mutex};

            ensureNotDone();
            noticeQueue.push(prodIndex);
        }
    }

    void notify(const SegId& segId)
    {
        if (peerProto.getRmtNodeType() != NodeType::PUBLISHER) {
            Guard guard{mutex};

            ensureNotDone();
            noticeQueue.push(segId);
        }
    }

    void request(const ProdIndex prodIndex)
    {
        Guard guard{mutex};

        ensureNotDone();
        requestQueue.push(prodIndex);
    }

    void request(const SegId& segId)
    {
        Guard guard{mutex};

        ensureNotDone();
        requestQueue.push(segId);
    }

    /**
     * Handles the remote node transitioning from not having a path to the
     * source of data-products to having one. Won't be called if he remote
     * node is the publisher.
     *
     * Possibly called by `peerProto` *after* `PeerProto::operator()()` is
     * called.
     */
    void pathToPub()
    {
        if (peerProto.getRmtNodeType() != NodeType::PUBLISHER) {
            rmtHasPathToPub = true;
            recvPeerMgr.pathToPub(rmtAddr);
        }
    }

    /**
     * Handles the remote node transitioning from having a path to the source of
     * data-products to not having one.
     *
     * Possibly called by `peerProto` *after* `PeerProto::operator()()` is
     * called.
     */
    void noPathToPub()
    {
        if (peerProto.getRmtNodeType() != NodeType::PUBLISHER) {
            rmtHasPathToPub = false;
            recvPeerMgr.noPathToPub(rmtAddr);
        }
    }

    bool isPathToPub() const noexcept
    {
        return rmtHasPathToPub;
    }

    void available(ProdIndex prodIndex)
    {
        LOG_DEBUG("Accepting notice of information on product " +
                prodIndex.to_string());

        int entryState;

        ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &entryState);
            const bool yes = recvPeerMgr.shouldRequest(rmtAddr, prodIndex);
        ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &entryState);

        if (yes) {
            LOG_DEBUG("Sending request for information on product " +
                    prodIndex.to_string());
            peerProto.request(prodIndex);
        }
    }

    void available(const SegId& segId)
    {
        LOG_DEBUG("Accepting notice of data-segment %s",
                segId.to_string().data());

        int entryState;

        ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &entryState);
            const bool yes = recvPeerMgr.shouldRequest(rmtAddr, segId);
        ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &entryState);

        if (yes) {
            LOG_DEBUG("Sending request for data-segment %s",
                    segId.to_string().data());
            peerProto.request(segId);
        }
    }

    void hereIs(const ProdInfo& prodInfo)
    {
        LOG_DEBUG("Accepting information on product %s",
                prodInfo.getProdIndex().to_string().data());

        int entryState;

        ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &entryState);
            (void)recvPeerMgr.hereIs(rmtAddr, prodInfo);
        ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &entryState);
    }

    void hereIs(TcpSeg& seg)
    {
        LOG_DEBUG("Accepting data-segment %s",
                seg.getSegId().to_string().data());

        int entryState;

        ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &entryState);
            (void)recvPeerMgr.hereIs(rmtAddr, seg);
        ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &entryState);
    }
};

Peer::Peer(
        TcpSock&     sock,
        NodeType     lclNodeType,
        XcvrPeerMgr& peerMgr)
    : pImpl(new SubPeer(sock, lclNodeType, peerMgr))
{}

Peer::Peer(
        const SockAddr& rmtSrvrAddr,
        const NodeType  lclNodeType,
        XcvrPeerMgr&    peerMgr)
    : pImpl(new SubPeer(rmtSrvrAddr, lclNodeType, peerMgr))
{}

} // namespace
