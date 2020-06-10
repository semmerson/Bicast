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
#include "PeerSet.h"

#include "error.h"
#include "hycast.h"
#include "Thread.h"

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_set>
#include <unordered_map>
#include <thread>

namespace hycast {

class PeerSet::Impl
{
    typedef std::mutex                       Mutex;
    typedef std::lock_guard<Mutex>           Guard;
    typedef std::unique_lock<Mutex>          Lock;
    typedef std::condition_variable          Cond;
    typedef std::thread                      Thread;
    typedef std::unordered_map<Peer, Thread> ThreadMap;

    mutable Mutex      mutex;
    mutable Cond       cond;
    bool               done;
    ThreadMap          threads;
    std::queue<Peer>   inactivePeers;
    std::thread        reaperThread;
    PeerSetMgr&        peerSetMgr;

    /**
     * Executes a peer. Called by `std::thread()`.
     *
     * @param[in] peer  Peer to be executed. A copy is used instead of a
     *                  reference to obviate problems arising from the peer
     *                  being destroyed elsewhere.
     */
    void execute(Peer peer)
    {
        //LOG_DEBUG("Executing peer");
        try {
            peer();

            {
                Guard guard(mutex);
                inactivePeers.push(peer);
                cond.notify_one();
            }

            peerSetMgr.stopped(peer);
        }
        catch (const std::system_error& ex) {
            log_error(ex);
        }
        catch (const std::exception& ex) {
            log_note(ex);
        }
    }

    void reapPeers() {
        //LOG_DEBUG("Reaping terminated peers");
        Lock lock{mutex};

        // While not done or a peer exists
        while (!done || !threads.empty() || !inactivePeers.empty()) {
            // While there's no peer to reap but there could be
            while (inactivePeers.empty() && (!done || !threads.empty()))
                cond.wait(lock);

            if (!inactivePeers.empty()) {
                auto iter = threads.find(inactivePeers.front());
                assert(iter != threads.end());
                iter->second.join();
                threads.erase(iter);
                inactivePeers.pop();
            }
        }
    }

public:
    Impl(PeerSetMgr& peerSetMgr)
        : mutex()
        , cond()
        , done{false}
        , threads()
        , inactivePeers()
        , reaperThread()
        , peerSetMgr(peerSetMgr)
    {}

    ~Impl()
    {
        LOG_TRACE();
        {
            Guard guard{mutex};

            done = true;

            // `execute()` would hang trying to de-activate the entry
            for (auto& pair : threads)
                pair.first.halt();

            cond.notify_one();
        }

        reaperThread.join();
    }

    /**
     * Executes a peer and adds it to the set of active peers.
     *
     * @param[in] peer        Peer to be activated
     * @throws    LogicError  Peer is already running
     * @threadsafety          Safe
     * @exceptionSafety       Strong guarantee
     * @cancellationpoint     No
     */
    void activate(const Peer peer)
    {
        Guard    guard(mutex);

        if (threads.count(peer))
            throw LOGIC_ERROR("Peer " + peer.to_string() + " is already "
                    "running");

        Canceler canceler{false};

        threads.emplace(std::piecewise_construct,
            std::forward_as_tuple(peer),
            std::forward_as_tuple(std::thread(&Impl::execute, this, peer)));
    }

    size_t size() const noexcept
    {
        Guard guard(mutex);
        return threads.size() - inactivePeers.size();
    }

    void notify(ProdIndex prodIndex)
    {
        Guard guard(mutex);

        for (auto& pair : threads)
            pair.first.notify(prodIndex);
    }

    void notify(const SegId& segId)
    {
        Guard guard(mutex);

        for (auto& pair : threads)
            pair.first.notify(segId);
    }

    void notify(
            ProdIndex   prodIndex,
            const Peer& notPeer)
    {
        Guard guard(mutex);

        for (auto& pair : threads) {
            Peer peer = pair.first;
            if (peer != notPeer)
                peer.notify(prodIndex);
        }
    }

    void notify(
            const SegId& segId,
            const Peer& notPeer)
    {
        Guard guard(mutex);

        for (auto& pair : threads) {
            Peer peer = pair.first;
            if (peer != notPeer)
                peer.notify(segId);
        }
    }

    void gotPath(Peer notPeer)
    {
        Guard guard(mutex);

        for (auto& pair : threads) {
            const Peer& peer = *static_cast<const Peer*>(&pair.first);
            if (peer != notPeer)
                peer.gotPath();
        }
    }

    void lostPath(Peer notPeer)
    {
        Guard guard(mutex);

        for (auto& pair : threads) {
            const Peer& peer = *static_cast<const Peer*>(&pair.first);
            if (peer != notPeer)
                peer.lostPath();
        }
    }
};

PeerSet::PeerSet(PeerSetMgr& peerSetMgr)
    : pImpl(new Impl(peerSetMgr)) {
}

void PeerSet::activate(const Peer peer) {
    return pImpl->activate(peer);
}

size_t PeerSet::size() const noexcept {
    return pImpl->size();
}

void PeerSet::gotPath(Peer notPeer) {
    pImpl->gotPath(notPeer);
}

void PeerSet::lostPath(Peer notPeer) {
    pImpl->lostPath(notPeer);
}

void PeerSet::notify(const ProdIndex prodIndex) {
    pImpl->notify(prodIndex);
}

void PeerSet::notify(
        const ProdIndex prodIndex,
        const Peer&     notPeer) {
    pImpl->notify(prodIndex, notPeer);
}

void PeerSet::notify(const SegId& segId) {
    pImpl->notify(segId);
}

void PeerSet::notify(
        const SegId& segId,
        const Peer&  notPeer) {
    pImpl->notify(segId, notPeer);
}

} // namespace
