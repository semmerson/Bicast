/**
 * This file implements a set of active peers whose remote counterparts can all
 * be notified together.
 *
 *  @file:  PeerSet.cpp
 * @author: Steven R. Emmerson <emmerson@ucar.edu>
 *
 *    Copyright 2021 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "config.h"

#include "HycastProto.h"
#include "logging.h"
#include "NoticeArray.h"
#include "PeerSet.h"
#include "ThreadException.h"

#include <map>
#include <pthread.h>
#include <unordered_map>
#include <utility>

namespace hycast {

/**
 * Thread-safe set of active peers.
 */
class PeerSet::Impl
{
    /**
     * A thread-safe class that sends notifications in a notice-queue to a
     * single peer.
     */
    class PeerEntry {
        mutable Mutex    mutex;
        mutable ThreadEx threadEx;
        Peer             peer;
        NoticeArray      noticeArray;
        ArrayIndex       readIndex;
        Thread           thread;

        void runNotifier() {
            try {
                for (;;) {
                    /*
                     * Because the following blocks indefinitely, the current
                     * thread must be cancelled in order to stop it and this
                     * must be done before this instance is destroyed.
                     *
                     */
                    if (!noticeArray.send(readIndex, peer))
                        break; // Connection lost
                    /*
                     * To avoid prematurely purging the current notice, the
                     * read-index must be incremented *after* the notice has
                     * been sent.
                     */
                    ++readIndex;
                }
            }
            catch (const std::exception& ex) {
                LOG_ERROR(ex);
                threadEx.set(ex);
            }
        }

    public:
        PeerEntry(Peer        peer,
                  NoticeArray noticeArray)
            : mutex()
            , threadEx()
            , peer(peer)
            , noticeArray(noticeArray)
            , readIndex(noticeArray.getOldestIndex())
            , thread(&PeerEntry::runNotifier, this)
        {}

        PeerEntry(const PeerEntry& peerEntry) =delete;
        PeerEntry& operator=(const PeerEntry& entry) =delete;

        PeerEntry(PeerEntry&& peerEntry) =default;

        ~PeerEntry() {
            if (thread.joinable()) {
                /*
                 * Canceling the thread should be safe because it's only sending
                 * notifications to the remote peer.
                 */
                ::pthread_cancel(thread.native_handle());
                thread.join();
            }

            peer.stop();
        }

        ArrayIndex getReadIndex() const {
            Guard guard(mutex);
            threadEx.throwIfSet();
            return readIndex;
        }
    };

    using PeerEntries = std::map<Peer, PeerEntry>;

    mutable Mutex mutex;
    mutable Cond  cond;
    // Placed before peer entries to ensure existence for `PeerEntry.run()`
    NoticeArray   noticeArray;
    PeerEntries   peerEntries;

    /**
     * Purges notice-queue of notices that have been read by all peers.
     */
    void purge() {
        const auto writeIndex = noticeArray.getWriteIndex();
        // Guaranteed to be equal to or greater than oldest read-index:
        auto       oldestIndex = writeIndex;

        // Find oldest read-index
        {
            Guard guard(mutex); // No changes allowed to peer-set
            for (const auto& peerEntry : peerEntries) {
                const auto readIndex = peerEntry.second.getReadIndex();

                if (readIndex < oldestIndex)
                    oldestIndex = readIndex;
            }
        }

        if (oldestIndex < writeIndex)
            // Purge notice-queue of entries that have been read by all peers
            noticeArray.eraseTo(oldestIndex);
    }

public:
    Impl(P2pMgr& p2pMgr)
        : mutex()
        , noticeArray(p2pMgr)
        , peerEntries()
    {}

    /**
     * Adds a started peer.
     *
     * @param[in] peer           Peer to be added
     * @throw LogicError         Peer was previously added
     * @throw LogicError         Remote peer belongs to the publisher
     * @throw std::system_error  Couldn't create new thread
     */
    void insert(Peer peer) {
        Guard guard(mutex);

        if (peerEntries.count(peer))
            throw LOGIC_ERROR("Peer " + peer.to_string() + " was previously "
                    "added");

        // NB: The following requires that `peer.hash()` works now
        const auto  pair = peerEntries.emplace(std::piecewise_construct,
                std::forward_as_tuple(peer),
                std::forward_as_tuple(peer, noticeArray));
        LOG_ASSERT(pair.second); // Because `peerEntries.count(peer) != 0`

        cond.notify_all();
    }

    void waitForPeer() {
        Lock lock{mutex};
        cond.wait(lock, [&]{return !peerEntries.empty();});
    }

    size_t size() {
        Guard guard{mutex};
        return peerEntries.size();
    }

    bool erase(Peer peer) {
        Guard guard(mutex);
        return peerEntries.erase(peer);
    }

    PeerEntries::size_type size() const {
        Guard guard(mutex);
        return peerEntries.size();
    }

    bool notify(const ProdIndex notice) {
        bool success = false;
        if (!peerEntries.empty()) {
            purge();
            noticeArray.putProdIndex(notice);
            success = true;
        }
        return success;
    }

    bool notify(const DataSegId notice) {
        bool success = false;
        if (!peerEntries.empty()) {
            purge();
            noticeArray.put(notice);
            success = true;
        }
        return success;
    }
};

/******************************************************************************/

PeerSet::PeerSet(P2pMgr& p2pMgr)
    : pImpl{new Impl(p2pMgr)}
{}

void PeerSet::insert(Peer peer) const {
    pImpl->insert(peer);
}

void PeerSet::waitForPeer() const {
    pImpl->waitForPeer();
}

bool PeerSet::erase(Peer peer) const {
    return pImpl->erase(peer);
}

PeerSet::size_type PeerSet::size() const {
    return pImpl->size();
}

bool PeerSet::notify(const ProdIndex notice) const {
    return pImpl->notify(notice);
}

bool PeerSet::notify(const DataSegId notice) const {
    return pImpl->notify(notice);
}

} // namespace
