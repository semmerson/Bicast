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
    typedef std::exception_ptr      ExceptPtr;

    mutable Mutex    doneMutex;
    mutable Cond     doneCond;
    ChunkIdQueue     noticeQueue;
    ChunkIdQueue     requestQueue;
    PeerProto        peerProto;
    const SockAddr   rmtAddr;
    PeerObs&         peerObs;
    ExceptPtr        exceptPtr;
    bool             haltRequested;

    void handleException(const ExceptPtr& exPtr)
    {
        Guard guard(doneMutex);

        if (!exceptPtr) {
            exceptPtr = exPtr;
            doneCond.notify_one();
        }
    }

    void runNotifier(void)
    {
        try {
            for (;;)
                noticeQueue.pop().notify(peerProto);
        }
        catch (const std::exception& ex) {
            handleException(std::current_exception());
        }
    }

    void runRequester(void)
    {
        try {
            for (;;)
                requestQueue.pop().request(peerProto);
        }
        catch (const std::exception& ex) {
            handleException(std::current_exception());
        }
    }

    void waitUntilDone()
    {
        Lock lock(doneMutex);

        while (!haltRequested && !exceptPtr)
            doneCond.wait(lock);
    }

public:
    /**
     * Constructs from a connection to a remote peer and a receiver of messages
     * from the local peer.
     *
     * @param[in] peerConn  Connection to the remote peer
     * @param[in] msgRcvr   The receiver of messages from the local peer
     */
    Impl(   PeerProto& peerProto,
            PeerObs&   msgRcvr)
        : doneMutex()
        , doneCond()
        , noticeQueue{}
        , requestQueue{}
        , peerProto{peerProto}
        , rmtAddr{peerProto.getRmtAddr()}
        , peerObs(msgRcvr) // Braces don't work
        , exceptPtr()
        , haltRequested{false}
    {
        peerProto.set(this);
    }

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
        std::thread notifierThread{&Impl::runNotifier, this};

        try {
            std::thread requesterThread{&Impl::runRequester, this};

            try {
                peerProto();

                {
                    Guard guard{doneMutex};
                    if (!haltRequested && exceptPtr)
                        std::rethrow_exception(exceptPtr);
                }

                (void)pthread_cancel(requesterThread.native_handle());
                requesterThread.join();
            }
            catch (...) {
                (void)pthread_cancel(requesterThread.native_handle());
                requesterThread.join();
                throw;
            }

            (void)pthread_cancel(notifierThread.native_handle());
            notifierThread.join();
        }
        catch (...) {
            (void)pthread_cancel(notifierThread.native_handle());
            notifierThread.join();
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
        Guard guard(doneMutex);

        haltRequested = true;
        peerProto.halt();
        doneCond.notify_one();
    }

    void notify(const ChunkId chunkId)
    {
        noticeQueue.push(chunkId);
    }

    void request(const ChunkId chunkId)
    {
        requestQueue.push(chunkId);
    }

    void request(const ProdId prodId)
    {
        requestQueue.push(prodId);
    }

    void request(const SegId& segId)
    {
        requestQueue.push(segId);
    }

    std::string to_string() const noexcept
    {
        return peerProto.to_string();
    }

    void acceptNotice(const ChunkId chunkId)
    {
        LOG_DEBUG("Accepting notice of chunk %s", chunkId.to_string().data());

        int entryState;

        ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &entryState);
            const bool doRequest = peerObs.shouldRequest(chunkId, rmtAddr);
        ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &entryState);

        if (doRequest) {
            LOG_DEBUG("Sending request for chunk %s",
                    chunkId.to_string().data());
            chunkId.request(peerProto);
        }
    }

    void acceptRequest(const ChunkId chunkId)
    {
        LOG_DEBUG("Accepting request for chunk %s", chunkId.to_string().data());

        int entryState;

        ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &entryState);
            const Chunk& chunk(peerObs.get(chunkId, rmtAddr));
        ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &entryState);

        if (chunk) {
            LOG_DEBUG("Sending chunk %s", chunkId.to_string().data());
            chunk.send(peerProto);
        }
    }

    void accept(const ProdInfo& prodInfo)
    {
        LOG_DEBUG("Accepting information on product %lu",
                static_cast<unsigned long>(prodInfo.getIndex()));

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
        PeerProto& peerProto,
        PeerObs&   msgRcvr)
    : pImpl{new Impl(peerProto, msgRcvr)}
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

void Peer::halt() noexcept
{
    if (pImpl)
        pImpl->halt();
}

void Peer::notify(const ChunkId chunkId) const
{
    pImpl->notify(chunkId);
}

void Peer::request(const ChunkId chunkId) const
{
    pImpl->request(chunkId);
}

void Peer::request(const ProdId prodId) const
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

void Peer::acceptNotice(const ChunkId chunkId)
{
    pImpl->acceptNotice(chunkId);
}

void Peer::acceptRequest(const ChunkId chunkId)
{
    pImpl->acceptRequest(chunkId);
}

void Peer::accept(const ProdInfo& prodInfo)
{
    pImpl->accept(prodInfo);
}

void Peer::accept(TcpSeg& seg)
{
    pImpl->accept(seg);
}


} // namespace
