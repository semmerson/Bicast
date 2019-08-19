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

#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <thread>

namespace hycast {

class PeerSet::Impl {
    class Entry;

    typedef std::lock_guard<std::mutex> Guard;
    typedef std::shared_ptr<Entry>      EntryPtr;

    class Entry
    {
        mutable Peer        peer;
        uint_fast32_t       chunkCount;
        mutable std::thread thread;

    public:
        Entry(const Peer& peer)
            : peer(peer)
            , chunkCount{0}
            , thread{}
        {}

        ~Entry() noexcept
        {
            if (thread.joinable()) {
                peer.terminate(); // Idempotent
                thread.join();
            }
        }

        void execute(Impl* impl) const
        {
            thread = std::thread(&Impl::execute, impl, peer);
        }

        bool operator <(const Entry& rhs) const noexcept
        {
            return peer < rhs.peer;
        }

        bool notify(const ChunkId& chunkId) const
        {
            return peer.notify(chunkId);
        }
    };

    mutable std::mutex                        mutex;
    mutable std::condition_variable           cond;
    bool                                      done;
    PeerSet::Observer*                        observer;
    std::thread                               thread;
    std::unordered_map<Peer, EntryPtr>        active;
    std::queue<EntryPtr, std::list<EntryPtr>> inactive;

    void joinPeerThreads() {
        std::unique_lock<std::mutex> lock{mutex};

        for (;;) {
            while (!done && inactive.empty())
                cond.wait(lock);

            if (done)
                break;

            inactive.pop(); // Entry's destructor joins peer's execution thread
        }
    }

public:
    Impl(PeerSet::Observer* observer = nullptr)
        : mutex()
        , cond()
        , done{false}
        , observer(observer)
        , thread()
        , active()
        , inactive()
    {
        thread = std::thread(&Impl::joinPeerThreads, this);
    }

    ~Impl() {
        {
            Guard guard{mutex};

            done = true;
            cond.notify_one();
        }

        thread.join();
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
        //LOG_NOTE("Executing peer");
        try {
            peer();
        }
        catch (const std::exception& ex) {
            log_error(ex);
        }

        {
            Guard guard(mutex);
            auto  iter = active.find(peer);
            auto  entryPtr = iter->second;

            active.erase(iter);
            inactive.push(entryPtr);
            cond.notify_one();
        }

        if (observer) {
            try {
                observer->stopped(peer);
            }
            catch (const std::exception& ex) {
                log_error(ex);
            }
        }
    }

    bool activate(const Peer& peer)
    {
        Guard guard(mutex);
        auto  pair = active.insert(std::pair<Peer, EntryPtr>(
                peer, EntryPtr(new Entry(peer))));

        if (pair.second)
            pair.first->second->execute(this);

        return pair.second;
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

bool PeerSet::activate(const Peer& peer)
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
