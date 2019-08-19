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

    NoticeQueue   notices;
    OutChunks     outChunks;
    RemotePeer    rmtPeer;
    PeerMsgRcvr&  msgRcvr;
    ExceptPtr     exceptPtr;
    mutable Mutex stateMutex;
    mutable Cond  stateCond;
    std::thread   noticeThread;
    std::thread   requestThread;
    std::thread   chunkThread;
    std::thread   notifyThread;
    Peer*         peer;
    State         state;

    /**
     * Constructs from a remote peer and a receiver of messages from the remote
     * peer.
     *
     * @param[in] rmtPeer  The remote peer
     * @param[in] msgRcvr  The receiver of messages from the remote peer
     */
    Impl(   RemotePeer&& rmtPeer,
            PeerMsgRcvr& msgRcvr)
        : notices()
        , outChunks()
        , rmtPeer{rmtPeer}
        , msgRcvr(msgRcvr) // Braces don't work
        , exceptPtr()
        , stateMutex()
        , stateCond()
        , noticeThread()
        , requestThread()
        , chunkThread()
        , notifyThread()
        , peer{nullptr}
        , state{INIT}
    {}

    void handleException(const ExceptPtr& exPtr)
    {
        Guard guard(stateMutex);

        if (!exceptPtr) {
            exceptPtr = exPtr;
            stateCond.notify_one();
        }
    }

    void recvNotices()
    {
        try {
            int entryState;

            ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &entryState);

            for (;;) {
                ChunkId chunkId = rmtPeer.getNotice();

                int cancelState;

                ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancelState);
                    const bool doRequest = msgRcvr.shouldRequest(chunkId,
                            *peer);
                ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &cancelState);

                if (doRequest) {
                    outChunks.insert(chunkId);
                    rmtPeer.request(chunkId);
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
        try {
            int entryState;

            ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &entryState);

            for (;;) {
                ChunkId chunkId = rmtPeer.getRequest();

                int cancelState;

                ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancelState);
                    MemChunk chunk = msgRcvr.get(chunkId, *peer);
                ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &cancelState);

                if (chunk)
                    rmtPeer.send(chunk);
            }

            ::pthread_setcancelstate(entryState, &entryState);
        }
        catch (const std::exception& ex) {
            handleException(std::current_exception());
        }
    }

    void recvChunks()
    {
        try {
            int entryState;

            ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &entryState);

            for (;;) {
                WireChunk chunk = rmtPeer.getChunk();

                int cancelState;

                ::pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancelState);
                    msgRcvr.hereIs(chunk, *peer);
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
        try {
            int entryState;

            ::pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &entryState);

            for (;;)
                rmtPeer.notify(notices.pop());

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
        chunkThread = std::thread(&Impl::recvChunks, this);
        try {
            noticeThread = std::thread(&Impl::recvNotices, this);
            try {
                requestThread = std::thread(&Impl::recvRequests, this);
                try {
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
        Lock lock(stateMutex);

        while (state != STOP_REQUESTED && !exceptPtr)
            stateCond.wait(lock);
    }

    /**
     * Idempotent.
     */
    void stopTasks()
    {
        int status;

        if (notifyThread.joinable()) {
            status = ::pthread_cancel(notifyThread.native_handle());
            if (status)
                LOG_INFO("Couldn't cancel notify-thread: %s", ::strerror(status));
            notifyThread.join();
        }
        if (requestThread.joinable()) {
            status = ::pthread_cancel(requestThread.native_handle());
            if (status)
                LOG_INFO("Couldn't cancel request-thread: %s", ::strerror(status));
            requestThread.join();
        }
        if (noticeThread.joinable()) {
            status = ::pthread_cancel(noticeThread.native_handle());
            if (status)
                LOG_INFO("Couldn't cancel notice-thread: %s", ::strerror(status));
            noticeThread.join();
        }
        if (chunkThread.joinable()) {
            status = ::pthread_cancel(chunkThread.native_handle());
            if (status)
                LOG_INFO("Couldn't cancel chunk-thread: %s", ::strerror(status));
            chunkThread.join();
        }
    }

public:
    typedef OutChunks::iterator iterator;

    /**
     * Server-side construction.
     *
     * @param[in] sock      `::accept()`ed socket
     * @param[in] portPool  Pool of potential port numbers
     * @param[in] msgRcvr   The receiver of messages from the remote peer
     */
    Impl(   Socket&      sock,
            PortPool&    portPool,
            PeerMsgRcvr& msgRcvr)
        : Impl(RemotePeer{sock, portPool}, msgRcvr)
    {}

    /**
     * Client-side construction. Potentially slow because a connection is
     * established with the remote peer in order to be symmetrical with server-
     * side construction, in which the connection already exists.
     *
     * @param[in] srvrAddr  Socket address of the remote server
     * @param[in] msgRcvr   The receiver of messages from the remote peer
     */
    Impl(   const SockAddr& srvrAddr,
            PeerMsgRcvr&    msgRcvr)
        : Impl(RemotePeer(srvrAddr), msgRcvr)
    {}

#if 0
    ~Impl() noexcept
    {
        try {
            stopTasks(); // Idempotent
        }
        catch (const std::exception& ex) {
            log_error(ex);
        }
    }
#endif

    /*
     * Couldn't set `peer` in the constructor. Tried
     *   - Passing in `this` and `*this`
     *   - Using `std::move()`
     *   - Using a reference
     * Either it wouldn't compile, Peer::request() would call a pure virtual
     * function, or `peer.pImpl` would be empty.
     */
    void setPeer(Peer* peer)
    {
        this->peer = peer;
    }

    /**
     * Executes this instance by starting subtasks. Doesn't return until an
     * exception is thrown by a subtask or `terminate()` is called. Upon return,
     * all subtasks have terminated. If `terminate()` is called before this
     * method, then this instance will return immediately and won't execute.
     */
    void operator ()()
    {
        Lock lock(stateMutex);

        if  (state == INIT) {
            startTasks();

            try {
                state = STARTED;
                lock.unlock();

                waitUntilDone();
                stopTasks();
                rmtPeer.disconnect(); // Idempotent

                lock.lock();
                if (state != STOP_REQUESTED && exceptPtr)
                    std::rethrow_exception(exceptPtr);
            } // Tasks started
            catch (const std::exception& ex) {
                stopTasks();
                throw;
            }
        }
    }

    /**
     * Halts execution. Does nothing if `operator()()` has not been called;
     * otherwise, causes `operator()()` to return and disconnects from the
     * remote peer. Idempotent.
     */
    void terminate() noexcept
    {
        Guard guard(stateMutex);

        if (state == STARTED) {
            state = STOP_REQUESTED;
            stateCond.notify_one();
        }
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
        return rmtPeer.to_string();
    }
};

/******************************************************************************/

Peer::Peer()
    : pImpl{}
{}

Peer::Peer(
        Socket&      sock,
        PortPool&    portPool,
        PeerMsgRcvr& msgRcvr)
    : pImpl{new Impl(sock, portPool, msgRcvr)}
{
    pImpl->setPeer(this);
}

Peer::Peer(
        const SockAddr& srvrAddr,
        PeerMsgRcvr& msgRcvr)
    : pImpl{new Impl(srvrAddr, msgRcvr)}
{
    pImpl->setPeer(this);
}

Peer::Peer(const Peer& peer)
    : pImpl(peer.pImpl)
{}

Peer::operator bool() noexcept {
    return static_cast<bool>(pImpl);
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

void Peer::terminate() noexcept
{
    if (pImpl)
        pImpl->terminate();
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
