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

class PeerSet::Impl {
    typedef std::lock_guard<std::mutex> Guard;

    mutable std::mutex                    mutex;
    mutable std::condition_variable       cond;
    bool                                  done;
    PeerSet::Observer*                    observer;
    std::unordered_map<Peer, std::thread> threads;
    std::queue<Peer>                      inactivePeers;
    std::thread                           reaperThread;

    void reapPeers() {
        //LOG_DEBUG("Reaping terminated peers");
        std::unique_lock<std::mutex> lock{mutex};

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
    Impl(PeerSet::Observer* observer = nullptr)
        : mutex()
        , cond()
        , done{false}
        , observer(observer)
        , threads()
        , inactivePeers()
        , reaperThread()
    {
        //LOG_DEBUG("Creating reaper thread");
        reaperThread = std::thread(&Impl::reapPeers, this);
    }

    ~Impl() {
        LOG_TRACE();
        {
            Guard guard{mutex};

            done = true;

            // `execute()` will hang trying to de-activate the entry
            for (auto& pair : threads)
                pair.first.halt();

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
            auto  iter = threads.find(peer);
            assert(iter != threads.end());
            inactivePeers.push(iter->first);
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
            auto     pair = threads.emplace(std::piecewise_construct,
                std::forward_as_tuple(peer),
                std::forward_as_tuple(std::thread(&Impl::execute, this, peer)));
        }

        return success;
    }

    void notify(ProdIndex prodIndex)
    {
        Guard guard(mutex);

        for (auto& pair : threads)
            pair.first.notify(prodIndex);
    }

    void notifyExcept(
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

    void notify(const SegId& segId)
    {
        Guard guard(mutex);

        for (auto& pair : threads)
            pair.first.notify(segId);
    }

    void notifyExcept(
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

    size_t size() const noexcept
    {
        Guard guard(mutex);
        return threads.size() - inactivePeers.size();
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

void PeerSet::notify(const ProdIndex prodIndex)
{
    pImpl->notify(prodIndex);
}

void PeerSet::notify(
        const ProdIndex prodIndex,
        const Peer&     notPeer)
{
    pImpl->notifyExcept(prodIndex, notPeer);
}

void PeerSet::notify(const SegId& segId)
{
    pImpl->notify(segId);
}

void PeerSet::notify(
        const SegId& segId,
        const Peer&  notPeer)
{
    pImpl->notifyExcept(segId, notPeer);
}

size_t PeerSet::size() const noexcept
{
    return pImpl->size();
}

} // namespace
