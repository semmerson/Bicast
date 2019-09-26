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

#include "Chunk.h"
#include "error.h"
#include "NoticeQueue.h"
#include "Peer.h"

#include <atomic>
#include <cstring>
#include <condition_variable>
#include <exception>
#include <functional>
#include <list>
#include <mutex>
#include <pthread.h>
#include <queue>
#include <thread>
#include <unordered_set>

namespace hycast {

class Peer::Impl
{
private:
    typedef std::mutex              Mutex;
    typedef std::lock_guard<Mutex>  Guard;
    typedef std::unique_lock<Mutex> Lock;
    typedef std::condition_variable Cond;
    typedef std::exception_ptr      ExceptPtr;
    typedef enum {
        INIT,
        STARTED,
        STOP_REQUESTED
    }                               State;

    // Outstanding/pending chunks (i.e., requested chunks that haven't arrived)
    class OutChunks
    {
    private:
        typedef std::unordered_set<ChunkId> Chunks;

        mutable Mutex  mutex;
        Chunks         chunks; // Outstanding/pending chunks

    public:
        typedef Chunks::const_iterator iterator;

        OutChunks()
            : mutex()
            , chunks()
        {}

        /**
         * Inserts a chunk identifier.
         *
         * @param[in] chunkId  Chunk identifier
         * @threadsafety       Safe
         */
        void insert(const ChunkId& chunkId)
        {
            Guard guard(mutex);
            chunks.insert(chunkId);
        }

        /**
         * Inserts a chunk identifier.
         *
         * @param[in] chunkId  Chunk identifier
         * @threadsafety       Safe
         */
        void insert(const ChunkId&& chunkId)
        {
            Guard guard(mutex);
            chunks.insert(chunkId);
        }

        /**
         * Removes a chunk identifier.
         *
         * @param[in] chunkId  Chunk identifier
         * @threadsafety       Safe
         */
        void erase(const ChunkId& chunkId)
        {
            Guard guard(mutex);
            chunks.erase(chunkId);
        }

        size_t size() const noexcept
        {
            Guard guard(mutex);
            return chunks.size();
        }

        iterator begin() const noexcept
        {
            Guard guard(mutex);
            return chunks.begin();
        }

        iterator end() const noexcept
        {
            Guard guard(mutex);
            return chunks.end();
        }
    };

    std::atomic_flag executing;
    mutable Mutex    doneMutex;
    mutable Cond     doneCond;
    NoticeQueue      notices;
    OutChunks        outChunks;
    PeerConn         peerConn;
    PeerMsgRcvr&     msgRcvr;
    ExceptPtr        exceptPtr;
    // A peer that references this instance and that's guaranteed to exist
    bool             haltRequested;
    std::thread      noticeThread;
    std::thread      requestThread;
    std::thread      chunkThread;
    std::thread      notifyThread;

    void handleException(const ExceptPtr& exPtr)
    {
        Guard guard(doneMutex);

        if (!exceptPtr) {
            exceptPtr = exPtr;
            doneCond.notify_one();
        }
    }

    void recvNotices()
    {
        LOG_DEBUG("Receiving notices");
        try {
            int entryState;

            ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &entryState);

            for (;;) {
                ChunkId chunkId = peerConn.getNotice();
                LOG_DEBUG("Received notice");

                //LOG_NOTE("Received chunk ID %lu", chunkId.id);

                int cancelState;

                ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancelState);
                    const bool doRequest = msgRcvr.shouldRequest(chunkId,
                            peerConn.getRmtAddr());
                ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &cancelState);

                if (doRequest) {
                    outChunks.insert(chunkId);
                    peerConn.request(chunkId);
                }
            }

            ::pthread_setcancelstate(entryState, &entryState);
        }
        catch (const std::exception& ex) {
            handleException(std::current_exception());
        }
    }

    void recvRequests()
    {
        LOG_DEBUG("Receiving requests");
        try {
            int entryState;

            ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &entryState);

            for (;;) {
                ChunkId chunkId = peerConn.getRequest();
                LOG_DEBUG("Received request");

                //LOG_NOTE("Received chunk ID %lu", chunkId.id);

                int cancelState;

                ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancelState);
                    MemChunk chunk = msgRcvr.get(chunkId, peerConn.getRmtAddr());
                ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &cancelState);

                if (chunk)
                    peerConn.send(chunk);
            }

            ::pthread_setcancelstate(entryState, &entryState);
        }
        catch (const std::exception& ex) {
            handleException(std::current_exception());
        }
    }

    void recvChunks()
    {
        LOG_DEBUG("Receiving chunks");
        try {
            int entryState;

            ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &entryState);

            for (;;) {
                WireChunk chunk = peerConn.getChunk();
                LOG_DEBUG("Received chunk");

                //LOG_NOTE("Received chunk %lu", chunk.getId().id);

                int cancelState;

                ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancelState);
                    msgRcvr.hereIs(chunk, peerConn.getRmtAddr());
                    outChunks.erase(chunk.getId());
                ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &cancelState);
            }

            ::pthread_setcancelstate(entryState, &entryState);
        }
        catch (const std::exception& ex) {
            handleException(std::current_exception());
        }
    }

    void sendNotices()
    {
        LOG_DEBUG("Sending notices");
        try {
            int entryState;

            ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &entryState);

            for (;;) {
                ChunkId chunkId = notices.pop();
                //LOG_NOTE("Notifying about chunk %lu", chunkId.id);
                peerConn.notify(chunkId);
                LOG_DEBUG("Sent notice");
            }

            ::pthread_setcancelstate(entryState, &entryState);
        }
        catch (const std::exception& ex) {
            handleException(std::current_exception());
        }
    }

    void startTasks()
    {
        /*
         * The tasks aren't detached because then they couldn't be canceled
         * (`std::thread::native_handle()` returns 0 for detached threads).
         */
        LOG_DEBUG("Creating receive-chunk thread");
        chunkThread = std::thread(&Impl::recvChunks, this);
        try {
            LOG_DEBUG("Creating receive-notice thread");
            noticeThread = std::thread(&Impl::recvNotices, this);
            try {
                LOG_DEBUG("Creating send-request thread");
                requestThread = std::thread(&Impl::recvRequests, this);
                try {
                    LOG_DEBUG("Creating send-notice thread");
                    notifyThread = std::thread(&Impl::sendNotices, this);
                }
                catch (const std::exception& ex) {
                    ::pthread_cancel(requestThread.native_handle());
                    throw;
                }
            } // `noticeThread` created
            catch (const std::exception& ex) {
                ::pthread_cancel(noticeThread.native_handle());
                throw;
            }
        } // `chunkThread` created
        catch (const std::exception& ex) {
            ::pthread_cancel(chunkThread.native_handle());
            throw;
        }
    }

    void waitUntilDone()
    {
        Lock lock(doneMutex);

        while (!haltRequested && !exceptPtr)
            doneCond.wait(lock);
    }

    /**
     * Idempotent.
     */
    void stopTasks()
    {
        int status;

        if (notifyThread.joinable()) {
            status = ::pthread_cancel(notifyThread.native_handle());
            if (status && status != ESRCH)
                LOG_ERROR("Couldn't cancel notify-thread: %s",
                        ::strerror(status));
            notifyThread.join();
        }
        if (requestThread.joinable()) {
            status = ::pthread_cancel(requestThread.native_handle());
            if (status && status != ESRCH)
                LOG_ERROR("Couldn't cancel request-thread: %s",
                        ::strerror(status));
            requestThread.join();
        }
        if (noticeThread.joinable()) {
            status = ::pthread_cancel(noticeThread.native_handle());
            if (status && status != ESRCH)
                LOG_ERROR("Couldn't cancel notice-thread: %s",
                        ::strerror(status));
            noticeThread.join();
        }
        if (chunkThread.joinable()) {
            status = ::pthread_cancel(chunkThread.native_handle());
            if (status && status != ESRCH)
                LOG_ERROR("Couldn't cancel chunk-thread: %s",
                        ::strerror(status));
            chunkThread.join();
        }
    }

public:
    typedef OutChunks::iterator iterator;

    /**
     * Server-side construction. Constructs from a remote peer and a receiver of
     * messages from the remote peer.
     *
     * @param[in] peerConn  The remote peer
     * @param[in] msgRcvr  The receiver of messages from the remote peer
     */
    Impl(   PeerConn     peerConn,
            PeerMsgRcvr& msgRcvr)
        : executing{ATOMIC_FLAG_INIT}
        , doneMutex()
        , doneCond()
        , notices()
        , outChunks()
        , peerConn{peerConn}
        , msgRcvr(msgRcvr) // Braces don't work
        , exceptPtr()
        , haltRequested{false}
        , noticeThread()
        , requestThread()
        , chunkThread()
        , notifyThread()
    {}

    /**
     * Client-side construction. Potentially slow because a connection is
     * established with the remote peer in order to be symmetrical with server-
     * side construction, in which the connection already exists.
     *
     * @param[in] srvrAddr            Socket address of the remote server
     * @param[in] portPool            Pool of potential port numbers for
     *                                temporary servers
     * @param[in] msgRcvr             Receiver of messages from the remote peer
     * @throws    std::system_error   System error
     * @throws    std::runtime_error  Remote peer closed the connection
     * @cancellationpoint             Yes
     */
    Impl(   const SockAddr& srvrAddr,
            PortPool&       portPool,
            PeerMsgRcvr&    msgRcvr)
        : Impl(PeerConn(srvrAddr, portPool), msgRcvr)
    {}

    ~Impl() noexcept
    {
        LOG_TRACE();

        if (noticeThread.joinable())
            LOG_ERROR("Notice thread is joinable");

        if (requestThread.joinable())
            LOG_ERROR("Request thread is joinable");

        if (chunkThread.joinable())
            LOG_ERROR("Chunk thread is joinable");

        if (notifyThread.joinable())
            LOG_ERROR("Notify thread is joinable");
    }

    /*
     * Couldn't set `peer` in the constructor. Tried
     *   - Passing in `this` and `*this`
     *   - Using `std::move()`
     *   - Using a reference
     * Either it wouldn't compile, Peer::request() would call a pure virtual
     * function, or `peer.pImpl` would be empty.
    void setPeer(Peer& peer)
    {
        this->peer = peer;
    }
     */

    /**
     * Returns the socket address of the remote peer. On the client-side, this
     * will be the address of the peer-server; on the server-side, this will be
     * the address of the `accept()`ed socket.
     *
     * @return            Socket address of the remote peer.
     * @cancellationpoint No
     */
    const SockAddr& getRmtAddr() const noexcept {
        return peerConn.getRmtAddr();
    }

    /**
     * Returns the local socket address.
     *
     * @return            Local socket address
     * @cancellationpoint No
     */
    const SockAddr& getLclAddr() const noexcept {
        return peerConn.getLclAddr();
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
        if (executing.test_and_set())
            throw LOGIC_ERROR("Already called");

        startTasks();

        try {
            waitUntilDone();
            stopTasks(); // Idempotent
            peerConn.disconnect(); // Idempotent

            Guard guard{doneMutex};
            if (!haltRequested && exceptPtr)
                std::rethrow_exception(exceptPtr);
        }
        catch (...) {
            stopTasks(); // Idempotent
            peerConn.disconnect(); // Idempotent
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
        doneCond.notify_one();
    }

    bool notify(const ChunkId& chunkId)
    {
        return notices.push(chunkId);
    }

    size_t size() const noexcept
    {
        return outChunks.size();
    }

    iterator begin() const noexcept
    {
        return outChunks.begin();
    }

    iterator end() const noexcept
    {
        return outChunks.end();
    }

    std::string to_string() const noexcept
    {
        return peerConn.to_string();
    }
};

/******************************************************************************/

Peer::Peer()
    : pImpl{}
{}

Peer::Peer(
        PeerConn     peerConn,
        PeerMsgRcvr& msgRcvr)
    : pImpl{new Impl(peerConn, msgRcvr)}
{
    //pImpl->setPeer(*this);
}

Peer::Peer(
        const SockAddr& srvrAddr,
        PortPool&       portPool,
        PeerMsgRcvr&    msgRcvr)
    : pImpl{new Impl(srvrAddr, portPool, msgRcvr)}
{
    //pImpl->setPeer(*this);
}

Peer::Peer(const Peer& peer)
    : pImpl(peer.pImpl)
{}

Peer::~Peer() noexcept
{}

const SockAddr& Peer::getRmtAddr() const noexcept {
    return pImpl->getRmtAddr();
}

const SockAddr& Peer::getLclAddr() const noexcept {
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

bool Peer::notify(const ChunkId& chunkId) const
{
    return pImpl->notify(chunkId);
}

size_t Peer::size() const noexcept
{
    return pImpl->size();
}

Peer::iterator Peer::begin() const noexcept
{
    return pImpl->begin();
}

Peer::iterator Peer::end() const noexcept
{
    return pImpl->end();
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
