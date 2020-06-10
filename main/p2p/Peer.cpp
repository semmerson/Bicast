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
protected:
    typedef std::mutex              Mutex;
    typedef std::lock_guard<Mutex>  Guard;
    typedef std::unique_lock<Mutex> Lock;
    typedef std::condition_variable Cond;
    typedef std::atomic<bool>       AtomicBool;
    typedef std::exception_ptr      ExceptPtr;

    mutable Mutex   mutex;           ///< State-change mutex
    mutable Cond    cond;            ///< State-change condition variable
    SendPeerMgr&    peerMgr;         ///< Peer manager
    ChunkIdQueue    noticeQueue;     ///< Queue for notices
    Peer&           peer;            ///< Containing `Peer`
    std::thread     notifierThread;  ///< Thread on which notices are sent
    std::thread     peerProtoThread; ///< Thread on which peerProto() executes
    ExceptPtr       exceptPtr;       ///< Pointer to terminating exception
    bool            done;            ///< Terminate without an exception?
    PeerProto&      peerProto;       ///< Peer-to-peer protocol object

    void handleException(const ExceptPtr& exPtr)
    {
        Guard guard(mutex);

        if (!exceptPtr) {
            exceptPtr = exPtr;
            cond.notify_one();
        }
    }

    void setDone()
    {
        Guard guard{mutex};
        done = true;
        cond.notify_one();
    }

    bool isDone()
    {
        Guard guard{mutex};
        return done;
    }

    void ensureNotDone()
    {
        if (done)
            throw LOGIC_ERROR("Peer has been halted");
    }

    void waitUntilDone()
    {
        Lock lock(mutex);

        while (!done && !exceptPtr)
            cond.wait(lock);
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
            handleException(std::current_exception());
        }
        catch (...) {
            LOG_DEBUG("Caught exception ...");
            throw;
        }
    }

    void startNotifierThread()
    {
        notifierThread = std::thread{&Impl::runNotifier, this};
    }

    void runPeerProto()
    {
        try {
            peerProto();
            // The remote peer closed the connection
            setDone();
        }
        catch (const std::exception& ex) {
            handleException(std::current_exception());
        }
    }

    void startPeerProtoThread()
    {
        peerProtoThread = std::thread{&Impl::runPeerProto, this};
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
     * @param[in] peer         Containing local peer
     */
    Impl(   PeerProto&&  peerProto,
            SendPeerMgr& peerMgr,
            Peer&        peer)
        : mutex()
        , cond()
        , peerMgr(peerMgr)
        , noticeQueue{}
        , peer(peer)
        , notifierThread{}
        , peerProtoThread{}
        , exceptPtr()
        , done{false}
        , peerProto(peerProto)
    {}

    virtual ~Impl() noexcept
    {
        LOG_TRACE();
    }

    /**
     * Returns the socket address of the remote peer. On the client-side, this
     * will be the address of the peer-server; on the server-side, this will be
     * the address of the `accept()`ed socket.
     *
     * @return            Socket address of the remote peer.
     * @cancellationpoint No
     */
    SockAddr getRmtAddr() const noexcept {
        return peerProto.getRmtAddr();
    }

    /**
     * Returns the local socket address.
     *
     * @return            Local socket address
     * @cancellationpoint No
     */
    SockAddr getLclAddr() const noexcept {
        return peerProto.getLclAddr();
    }

    /**
     * Executes this instance by starting subtasks. Doesn't return until an
     * exception is thrown by a subtask or `halt()` is called. Upon return,
     * all subtasks have terminated. If `halt()` is called before this method,
     * then this instance will return immediately and won't execute.
     *
     * @throws    std::system_error   System error
     * @throws    std::runtime_error  Remote peer closed the connection
     * @throws    std::logic_error    This method has already been called
     */
    void operator ()()
    {
        try {
            startTasks();

            try {
                waitUntilDone();
                stopTasks(); // Idempotent

                Guard guard{mutex};
                if (!done && exceptPtr)
                    std::rethrow_exception(exceptPtr);
            }
            catch (...) {
                stopTasks(); // Idempotent
                throw;
            }
        }
        catch (const std::exception& ex) {
            LOG_DEBUG("Caught \"%s\"", ex.what());
            std::throw_with_nested(RUNTIME_ERROR("Failure"));
        }
        catch (...) {
            LOG_DEBUG("Caught ...");
            throw;
        }
    }

    /**
     * Halts execution. Does nothing if `operator()()` has not been called;
     * otherwise, causes `operator()()` to return and disconnects from the
     * remote peer. Idempotent.
     *
     * @cancellationpoint No
     */
    void halt() noexcept
    {
        setDone();
    }

    std::string to_string() const noexcept
    {
        return peerProto.to_string();
    }

    /**
     * Indicates if this instance resulted from a call to `::connect()`.
     *
     * @retval `false`  No
     * @retval `true`   Yes
     */
    virtual bool isFromConnect() const noexcept =0;

    void notify(ProdIndex prodIndex)
    {
        Guard guard{mutex};

        ensureNotDone();
        noticeQueue.push(prodIndex);
    }

    void notify(const SegId& segId)
    {
        Guard guard{mutex};

        ensureNotDone();
        noticeQueue.push(segId);
    }

    void sendMe(const ProdIndex prodIndex)
    {
        LOG_DEBUG("Accepting request for product-information %s",
                prodIndex.to_string().data());

        int entryState;

        ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &entryState);
            auto prodInfo = peerMgr.getProdInfo(peer, prodIndex);
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

            int entryState;

            //::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &entryState);
                MemSeg memSeg = peerMgr.getMemSeg(peer, segId);
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

Peer::Peer(Impl* impl)
    : pImpl(impl) {
}

Peer::Peer(const Peer& peer)
    : pImpl(peer.pImpl) {
}

Peer::operator bool() const noexcept {
    return static_cast<bool>(pImpl);
}

const SockAddr Peer::getRmtAddr() const noexcept {
    return pImpl->getRmtAddr();
}

const SockAddr Peer::getLclAddr() const noexcept {
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
    void startTasks() override
    {
        startNotifierThread(); // Because `Impl::runNotifier` is protected!?

        try {
            startPeerProtoThread(); // Because `Impl::runPeerProto` is protected!?
        }
        catch (const std::exception& ex) {
            LOG_DEBUG("Caught \"%s\"", ex.what());
            (void)pthread_cancel(notifierThread.native_handle());
            notifierThread.join();
            throw;
        }
        catch (...) {
            LOG_DEBUG("Caught ...");
            (void)pthread_cancel(notifierThread.native_handle());
            notifierThread.join();
            throw;
        }
    }

    /**
     * Idempotent.
     */
    void stopTasks() override
    {
        if (peerProtoThread.joinable()) {
            peerProto.halt();
            peerProtoThread.join();
        }
        if (notifierThread.joinable()) {
            noticeQueue.close();
            //(void)pthread_cancel(notifierThread.native_handle());
            notifierThread.join();
        }
    }

public:
    /**
     * Constructs. Server-side construction only.
     *
     * @param[in] sock         TCP socket with remote peer
     * @param[in] portPool     Pool of port numbers for temporary servers
     * @param[in] peerMgr      Peer manager
     * @param[in] peer         Containing local peer
     */
    PubPeer(TcpSock&     sock,
            PortPool&    portPool,
            SendPeerMgr& peerMgr,
            Peer&        peer)
        : Peer::Impl(PeerProto(sock, portPool, *this), peerMgr, peer)
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

Peer::Peer(
        TcpSock&     sock,
        PortPool&    portPool,
        SendPeerMgr& peerMgr)
    : pImpl(new PubPeer(sock, portPool, peerMgr, *this)) {
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
    PeerProto       peerProto;       ///< Peer-to-peer protocol
    AtomicBool      rmtHasPathToPub; ///< Remote node has path to publisher?
    XcvrPeerMgr&    recvPeerMgr;     ///< Manager of subscriber peer
    Peer&           peer;            ///< Containing peer

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
            handleException(std::current_exception());
        }
    }

    void startTasks()
    {
        startNotifierThread();

        try {
            requesterThread = std::thread{&SubPeer::runRequester, this};

            try {
                startPeerProtoThread();
            }
            catch (const std::exception& ex) {
                LOG_DEBUG("Caught \"%s\"", ex.what());
                (void)pthread_cancel(requesterThread.native_handle());
                requesterThread.join();
                throw;
            }
            catch (...) {
                LOG_DEBUG("Caught ...");
                (void)pthread_cancel(requesterThread.native_handle());
                requesterThread.join();
                throw;
            }
        }
        catch (const std::exception& ex) {
            LOG_DEBUG("Caught \"%s\"", ex.what());
            (void)pthread_cancel(notifierThread.native_handle());
            notifierThread.join();
            throw;
        }
        catch (...) {
            LOG_DEBUG("Caught ...");
            (void)pthread_cancel(notifierThread.native_handle());
            notifierThread.join();
            throw;
        }
    }

    /**
     * Idempotent.
     */
    void stopTasks()
    {
        if (peerProtoThread.joinable()) {
            peerProto.halt();
            peerProtoThread.join();
        }
        if (requesterThread.joinable()) {
            requestQueue.close();
            //(void)pthread_cancel(requesterThread.native_handle());
            requesterThread.join();
        }
        if (notifierThread.joinable()) {
            noticeQueue.close();
            //(void)pthread_cancel(notifierThread.native_handle());
            notifierThread.join();
        }
    }

public:
    /**
     * Server-side construction (i.e., from an `::accept()`).
     *
     * @param[in] sock           TCP socket with remote peer
     * @param[in] portPool       Pool of port numbers for temporary servers
     * @param[in] lclNodeType    Type of local node
     * @param[in] peer           Containing local peer
     * @param[in] peerMgr        Peer Manager
     */
    SubPeer(TcpSock&        sock,
            PortPool&       portPool,
            const NodeType& lclNodeType,
            Peer&           peer,
            XcvrPeerMgr&    peerMgr)
        : Peer::Impl(PeerProto(sock, portPool, *this), peerMgr, peer)
        , fromConnect{false}
        , requestQueue{}
        , requesterThread{}
        , peerProto{sock, portPool, lclNodeType, *this}
        , rmtHasPathToPub{peerProto.getRmtNodeType()}
        , recvPeerMgr(peerMgr)
        , peer(peer)
    {}

    /**
     * Client-side construction (i.e., from a `::connect()`).
     *
     * @param[in] rmtSrvrAddr    Address of remote peer-server
     * @param[in] lclNodeType    Type of local node
     * @param[in] peer           Local peer that contains this instance
     * @param[in] subPeerMgrApi  Manager of subscriber peer
     * @throws    LogicError     `lclNodeType == NodeType::PUBLISHER`
     */
    SubPeer(const SockAddr& rmtSrvrAddr,
            const NodeType  lclNodeType,
            Peer&           peer,
            XcvrPeerMgr&    peerMgr)
        : Peer::Impl(PeerProto(rmtSrvrAddr, lclNodeType, *this), peerMgr, peer)
        , fromConnect{true}
        , requestQueue{}
        , requesterThread{}
        , peerProto{rmtSrvrAddr, lclNodeType, *this}
        , rmtHasPathToPub{peerProto.getRmtNodeType()}
        , recvPeerMgr(peerMgr)
        , peer(peer)
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
            recvPeerMgr.pathToPub(peer);
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
            recvPeerMgr.noPathToPub(peer);
        }
    }

    bool isPathToPub() const noexcept
    {
        return rmtHasPathToPub;
    }

    void available(ProdIndex prodIndex)
    {
        LOG_DEBUG("Accepting notice of product-information %s",
                prodIndex.to_string().data());

        int entryState;

        ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &entryState);
            const bool yes = recvPeerMgr.shouldRequest(peer, prodIndex);
        ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &entryState);

        if (yes) {
            LOG_DEBUG("Sending request for product-information %s",
                    prodIndex.to_string().data());
            peerProto.request(prodIndex);
        }
    }

    void available(const SegId& segId)
    {
        LOG_DEBUG("Accepting notice of data-segment %s",
                segId.to_string().data());

        int entryState;

        ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &entryState);
            const bool yes = recvPeerMgr.shouldRequest(peer, segId);
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
            (void)recvPeerMgr.hereIs(peer, prodInfo);
        ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &entryState);
    }

    void hereIs(TcpSeg& seg)
    {
        LOG_DEBUG("Accepting data-segment %s", seg.to_string().data());

        int entryState;

        ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &entryState);
            (void)recvPeerMgr.hereIs(peer, seg);
        ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &entryState);
    }
};

Peer::Peer(
        TcpSock&     sock,
        PortPool&    portPool,
        NodeType     lclNodeType,
        XcvrPeerMgr& peerMgr)
    : Peer(new SubPeer(sock, portPool, lclNodeType, *this, peerMgr)) {
}

Peer::Peer(
        const SockAddr& rmtSrvrAddr,
        const NodeType  lclNodeType,
        XcvrPeerMgr&    peerMgr)
    : Peer(new SubPeer(rmtSrvrAddr, lclNodeType, *this, peerMgr)) {
}

} // namespace
