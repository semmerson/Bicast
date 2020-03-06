/**
 * A local peer that communicates with its associated remote peer. Besides
 * sending notices to the remote peer, this class also creates and runs
 * independent threads that receive messages from a remote peer and pass them to
 * a peer message receiver.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Peer.cpp
 *  Created on: May 29, 2019
 *      Author: Steven R. Emmerson
 */
#include "config.h"
#include "Peer.h"

#include "ChunkIdQueue.h"
#include "error.h"
#include "hycast.h"
#include "PeerProto.h"

#include <condition_variable>
#include <exception>
#include <mutex>
#include <pthread.h>
#include <thread>

namespace hycast {

class Peer::Impl : public PeerProtoObs
{
private:
    typedef std::mutex              Mutex;
    typedef std::lock_guard<Mutex>  Guard;
    typedef std::unique_lock<Mutex> Lock;
    typedef std::condition_variable Cond;
    typedef std::atomic<bool>       AtomicBool;
    typedef std::exception_ptr      ExceptPtr;

    mutable Mutex  doneMutex;       ///< For protecting `done`
    mutable Cond   doneCond;        ///< For checking `done`
    ChunkIdQueue   noticeQueue;     ///< Queue for notices
    ChunkIdQueue   requestQueue;    ///< Queue for requests
    /// Remote site has path to source? Must be initialized before `peerProto`.
    AtomicBool     rmtHasPathToSrc;
    PeerProto      peerProto;       ///< Peer-to-peer protocol object
    const SockAddr rmtAddr;         ///< Address of remote peer
    PeerObs&       peerObs;         ///< Observer of this instance
    std::thread    notifierThread;  ///< Thread on which notices are sent
    std::thread    requesterThread; ///< Thread on which requests are made
    std::thread    peerProtoThread; ///< Thread on which peerProto() executes
    ExceptPtr      exceptPtr;       ///< Pointer to terminating exception
    bool           done;            ///< Terminate without an exception?
    Peer&          peer;            ///< Containing `Peer`

    void handleException(const ExceptPtr& exPtr)
    {
        Guard guard(doneMutex);

        if (!exceptPtr) {
            exceptPtr = exPtr;
            doneCond.notify_one();
        }
    }

    void setDone()
    {
        Guard guard{doneMutex};
        done = true;
        doneCond.notify_one();
    }

    bool isDone()
    {
        Guard guard{doneMutex};
        return done;
    }

    void waitUntilDone()
    {
        Lock lock(doneMutex);

        while (!done && !exceptPtr)
            doneCond.wait(lock);
    }

    void runNotifier(void)
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

    void runPeerProto(void)
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

    void startTasks()
    {
        notifierThread = std::thread{&Impl::runNotifier, this};

        try {
            requesterThread = std::thread{&Impl::runRequester, this};

            try {
                peerProtoThread = std::thread{&Impl::runPeerProto, this};
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
     * Server-side construction (i.e., from an `::accept()`). Constructs from a
     * connection to a remote peer and an observer of this instance.
     *
     * @param[in] sock      TCP socket with remote peer
     * @param[in] portPool  Pool of port numbers for temporary servers
     * @param[in] peerObs   Observer of this instance
     * @param[in] isSource  Is this instance the source of data-products?
     * @param[in] peer      Containing local peer
     */
    Impl(   TcpSock&   sock,
            PortPool&  portPool,
            PeerObs&   peerObs,
            const bool isSource,
            Peer&      peer)
        : doneMutex()
        , doneCond()
        , noticeQueue{}
        , requestQueue{}
        , rmtHasPathToSrc{false}
        , peerProto{sock, portPool, *this, isSource} // Might call `pathToSrc()`
        , rmtAddr{peerProto.getRmtAddr()}
        , peerObs(peerObs) // Braces don't work
        , notifierThread{}
        , requesterThread{}
        , peerProtoThread{}
        , exceptPtr()
        , done{false}
        , peer(peer)
    {}

    /**
     * Client-side construction (i.e., from a `::connect()`). Constructs from
     * the address of a remote peer- server and an observer of this instance.
     *
     * @param[in] rmtSrvrAddr  Address of remote peer-server
     * @param[in] peerObs      Observer of this instance
     * @param[in] peer         Containing local peer
     */
    Impl(   const SockAddr& rmtSrvrAddr,
            PeerObs&        peerObs,
            Peer&           peer)
        : doneMutex()
        , doneCond()
        , noticeQueue{}
        , requestQueue{}
        , rmtHasPathToSrc{false}
        , peerProto{rmtSrvrAddr, *this}
        , rmtAddr{peerProto.getRmtAddr()}
        , peerObs(peerObs) // Braces don't work
        , notifierThread{}
        , requesterThread{}
        , peerProtoThread{}
        , exceptPtr()
        , done{false}
        , peer(peer)
    {}

    ~Impl() noexcept
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
    const SockAddr& getRmtAddr() const noexcept {
        return rmtAddr;
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

                Guard guard{doneMutex};
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

    void notify(ProdIndex prodIndex)
    {
        noticeQueue.push(prodIndex);
    }

    void notify(const SegId& segId)
    {
        noticeQueue.push(segId);
    }

    void request(const ProdIndex prodIndex)
    {
        requestQueue.push(prodIndex);
    }

    void request(const SegId& segId)
    {
        requestQueue.push(segId);
    }

    std::string to_string() const noexcept
    {
        return peerProto.to_string();
    }

    /**
     * Called by `peerProto`.
     */
    void pathToSrc() noexcept
    {
        rmtHasPathToSrc = true;
    }

    /**
     * Called by `peerProto`.
     */
    void noPathToSrc() noexcept
    {
        rmtHasPathToSrc = false;
    }

    void acceptNotice(ProdIndex prodIndex)
    {
        LOG_DEBUG("Accepting notice of product-information %s",
                prodIndex.to_string().data());

        int entryState;

        ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &entryState);
            const bool doRequest = peerObs.shouldRequest(prodIndex, rmtAddr);
        ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &entryState);

        if (doRequest) {
            LOG_DEBUG("Sending request for product-information %s",
                    prodIndex.to_string().data());
            peerProto.request(prodIndex);
        }
    }

    void acceptNotice(const SegId& segId)
    {
        LOG_DEBUG("Accepting notice of data-segment %s", segId.to_string().data());

        int entryState;

        ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &entryState);
            const bool doRequest = peerObs.shouldRequest(segId, rmtAddr);
        ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &entryState);

        if (doRequest) {
            LOG_DEBUG("Sending request for data-segment %s",
                    segId.to_string().data());
            peerProto.request(segId);
        }
    }

    void acceptRequest(ProdIndex prodIndex)
    {
        LOG_DEBUG("Accepting request for product-information %s",
                prodIndex.to_string().data());

        int entryState;

        ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &entryState);
            auto prodInfo = peerObs.get(prodIndex, rmtAddr);
        ::pthread_setcancelstate(entryState, &entryState);

        if (prodInfo) {
            //LOG_DEBUG("Sending product-information %s",
                    //prodInfo.to_string().data());
            peerProto.send(prodInfo);
        }
    }

    void acceptRequest(const SegId& segId)
    {
        try {
            LOG_DEBUG("Accepting request for data-segment %s",
                    segId.to_string().data());

            int entryState;

            //::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &entryState);
                auto memSeg = peerObs.get(segId, rmtAddr);
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

    void accept(const ProdInfo& prodInfo)
    {
        LOG_DEBUG("Accepting information on product %s",
                prodInfo.getProdIndex().to_string().data());

        int entryState;

        ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &entryState);
            (void)peerObs.hereIs(prodInfo, rmtAddr);
        ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &entryState);
    }

    void accept(TcpSeg& seg)
    {
        LOG_DEBUG("Accepting data-segment %s", seg.to_string().data());

        int entryState;

        ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &entryState);
            (void)peerObs.hereIs(seg, rmtAddr);
        ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &entryState);
    }
};

/******************************************************************************/

Peer::Peer()
    : pImpl{}
{}

Peer::Peer(
        TcpSock&   sock,
        PortPool&  portPool,
        PeerObs&   peerObs,
        const bool isSource)
    : pImpl{new Impl(sock, portPool, peerObs, isSource, *this)}
{}

Peer::Peer(
        const SockAddr& rmtSrvrAddr,
        PeerObs&        peerObs)
    : pImpl{new Impl(rmtSrvrAddr, peerObs, *this)}
{}

Peer::Peer(const Peer& peer)
    : pImpl(peer.pImpl)
{}

Peer::~Peer() noexcept
{}

const SockAddr Peer::getRmtAddr() const noexcept {
    return pImpl->getRmtAddr();
}

const SockAddr Peer::getLclAddr() const noexcept {
    return pImpl->getLclAddr();
}

Peer::operator bool() const noexcept {
    return pImpl.operator bool();
}

Peer& Peer::operator=(const Peer& rhs)
{
    pImpl = rhs.pImpl;

    return *this;
}

bool Peer::operator==(const Peer& rhs) const noexcept
{
    return pImpl.get() == rhs.pImpl.get();
}

bool Peer::operator<(const Peer& rhs) const noexcept
{
    return pImpl.get() < rhs.pImpl.get();
}

void Peer::operator ()()
{
    pImpl->operator()();
}

void Peer::halt() const noexcept
{
    if (pImpl)
        pImpl->halt();
}

void Peer::notify(ProdIndex prodIndex) const
{
    pImpl->notify(prodIndex);
}

void Peer::notify(const SegId& segId) const
{
    pImpl->notify(segId);
}

void Peer::request(const ProdIndex prodId) const
{
    pImpl->request(prodId);
}

void Peer::request(const SegId& segId) const
{
    pImpl->request(segId);
}

size_t Peer::hash() const noexcept
{
    return std::hash<Impl*>()(pImpl.get());
}

std::string Peer::to_string() const noexcept
{
    return pImpl->to_string();
}

} // namespace
