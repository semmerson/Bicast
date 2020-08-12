/**
 * Thread-safe, dynamic set of active peers.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All Rights
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

#include <cassert>
#include <condition_variable>
#include <set>
#include <memory>
#include <mutex>
#include <thread>

namespace hycast {

class PeerSet::Impl
{
    using Mutex = std::mutex;
    using Cond = std::condition_variable;
    using Guard = std::lock_guard<Mutex>;
    using Lock = std::unique_lock<Mutex>;
    using Peers = std::set<Peer>;

    mutable Mutex      mutex;
    mutable Cond       cond;
    bool               done;
    Peers              peers;
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
        }
        catch (const std::system_error& ex) {
            log_error(ex);
        }
        catch (const std::exception& ex) {
            log_note(ex);
        }

        peerSetMgr.stopped(peer);

        {
            Guard guard{mutex};
            peers.erase(peer);
            if (peers.empty())
                cond.notify_one();
        }
    }

public:
    Impl(PeerSetMgr& peerSetMgr)
        : mutex{}
        , cond{}
        , done{false}
        , peers()
        , peerSetMgr(peerSetMgr)
    {}

    ~Impl()
    {
        Lock lock{mutex};
        done = true;
        for (auto& peer : peers)
            peer.halt();
        while (peers.size() > 0)
            cond.wait(lock);
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
        Guard    guard{mutex};

        if (!done) {
            if (peers.insert(peer).second) {
                Canceler canceler{false};
                auto thread = std::thread(&Impl::execute, this, peer);
                thread.detach();
            }
        }
    }

    size_t size() const noexcept
    {
        Guard guard{mutex};
        return peers.size();
    }

    void notify(ProdIndex prodIndex)
    {
        Guard guard{mutex};

        if (peers.empty()) {
            LOG_DEBUG("Peer set is empty");
        }
        else {
            for (auto& peer : peers)
                peer.notify(prodIndex);
        }
    }

    void notify(const SegId& segId)
    {
        Guard guard{mutex};

        if (peers.empty()) {
            LOG_DEBUG("Peer set is empty");
        }
        else {
            for (auto& peer : peers)
                peer.notify(segId);
        }
    }

    void notify(
            ProdIndex   prodIndex,
            const Peer& notPeer)
    {
        Guard guard{mutex};

        for (auto& peer : peers) {
            if (peer != notPeer)
                peer.notify(prodIndex);
        }
    }

    void notify(
            const SegId& segId,
            const Peer& notPeer)
    {
        Guard guard{mutex};

        for (auto& peer : peers) {
            if (peer != notPeer)
                peer.notify(segId);
        }
    }

    void gotPath(Peer notPeer)
    {
        Guard guard{mutex};

        for (auto& peer : peers) {
            if (peer != notPeer)
                peer.gotPath();
        }
    }

    void lostPath(Peer notPeer)
    {
        Guard guard{mutex};

        for (auto& peer : peers) {
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
