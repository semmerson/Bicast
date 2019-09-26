/**
 * Thread-safe, dynamic set of active peers.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: PeerSet.cpp
 *  Created on: Jun 7, 2019
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "error.h"
#include "PeerSet.h"
#include "Thread.h"

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <thread>

namespace hycast {

class PeerSet::Impl {
    class EntryImpl;

    typedef std::lock_guard<std::mutex> Guard;
    typedef std::shared_ptr<EntryImpl>  Entry;

    class EntryImpl
    {
        mutable Peer        peer;
        uint_fast32_t       chunkCount;
        mutable std::thread thread;

    public:
        EntryImpl(const Peer peer)
            : peer(peer)
            , chunkCount{0}
            , thread{}
        {}

        ~EntryImpl() noexcept
        {
            LOG_TRACE();
            if (thread.joinable()) {
                peer.halt(); // Idempotent
                thread.join();
            }
        }

        void execute(Impl* impl) const
        {
            LOG_DEBUG("Creating peer-execution thread");
            thread = std::thread(&Impl::execute, impl, peer);
        }

        bool operator <(const EntryImpl& rhs) const noexcept
        {
            return peer < rhs.peer;
        }

        bool notify(const ChunkId& chunkId) const
        {
            return peer.notify(chunkId);
        }

        void terminate()
        {
            peer.halt();
        }
    };

    mutable std::mutex                  mutex;
    mutable std::condition_variable     cond;
    bool                                done;
    PeerSet::Observer*                  observer;
    std::unordered_map<Peer, Entry>     active;
    std::queue<Entry, std::list<Entry>> inactive;
    std::thread                         reaperThread;

    void reapPeers() {
        LOG_DEBUG("Reaping peers");
        std::unique_lock<std::mutex> lock{mutex};

        // While not done or a peer exists
        while (!done || !active.empty() || !inactive.empty()) {
            // While there's no peer to reap but there could be
            while (inactive.empty() && (!done || !active.empty()))
                cond.wait(lock);

            if (!inactive.empty())
                inactive.pop(); // `~Entry()` joins peer's execution thread
        }
    }

public:
    Impl(PeerSet::Observer* observer = nullptr)
        : mutex()
        , cond()
        , done{false}
        , observer(observer)
        , active()
        , inactive()
        , reaperThread()
    {
        LOG_DEBUG("Creating reaper thread");
        reaperThread = std::thread(&Impl::reapPeers, this);
    }

    ~Impl() {
        LOG_TRACE();
        {
            Guard guard{mutex};
            auto  end = active.end();

            done = true;

            // `execute()` will hang trying to de-activate the entry
            for (auto iter = active.begin(); iter != end; ++iter)
                iter->second->terminate();

            cond.notify_one();
        }

        reaperThread.join();
    }

    /**
     * Executes a peer. Called by `std::thread()`.
     *
     * @param[in] peer  Peer to be executed. A copy is used instead of a
     *                  reference to obviate problems arising from the peer
     *                  being destroyed elsewhere.
     */
    void execute(Peer peer)
    {
        LOG_DEBUG("Executing peer");
        try {
            peer();
        }
        catch (const std::system_error& ex) {
            log_error(ex);
        }
        catch (const std::exception& ex) {
            log_note(ex);
        }

        // Must occur before destroying `peer` by removing it from active set
        if (observer) {
            try {
                observer->stopped(peer);
            }
            catch (const std::exception& ex) {
                log_error(ex);
            }
        }

        {
            Guard guard(mutex);
            auto  iter = active.find(peer);
            assert(iter != active.end());

            Entry entry = iter->second;

            active.erase(iter->first);
            inactive.push(entry);
            cond.notify_one();
        }
    }

    /**
     * @param[in] peer     Peer to be activated
     * @retval    `true`   Peer was successfully activated
     * @retval    `false`  Peer was already activated
     * @threadsafety       Safe
     * @exceptionSafety    Strong guarantee
     */
    bool activate(const Peer peer)
    {
        bool  success;
        Guard guard(mutex);

        if (done) {
            success = false;
        }
        else {
            Canceler canceler{false};
            auto     pair = active.insert(std::pair<Peer, Entry>(
                    peer, Entry(new EntryImpl(peer))));

            success = pair.second;

            if (success) {
                try {
                    pair.first->second->execute(this);
                    cond.notify_one();
                }
                catch (...) {
                    active.erase(peer);
                    throw;
                }
            }
        }

        return success;
    }

    bool notify(const ChunkId& chunkId)
    {
        Guard guard(mutex);
        bool  success = true;

        for (auto iter = active.begin(); iter != active.end(); ++iter)
            success &= iter->second->notify(chunkId);

        return success;
    }

    size_t size() const noexcept
    {
        Guard guard(mutex);
        return active.size();
    }
};

/******************************************************************************/

PeerSet::Observer::~Observer()
{}

PeerSet::PeerSet()
    : pImpl(new Impl())
{}

PeerSet::PeerSet(Observer& obs)
    : pImpl(new Impl(&obs))
{}

bool PeerSet::activate(const Peer peer)
{
    return pImpl->activate(peer);
}

bool PeerSet::notify(const ChunkId& chunkId)
{
    return pImpl->notify(chunkId);
}

size_t PeerSet::size() const noexcept
{
    return pImpl->size();
}

} // namespace
