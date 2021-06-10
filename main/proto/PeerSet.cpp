/**
 * This file implements a set of peers whose remote counterparts can all be
 * notified together.
 *
 *  @file: PeerSet.cpp
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
#include "NoticeQueue.h"
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
     * A thread-safe class responsible for sending notifications in a
     * notice-queue to a single peer.
     */
    class PeerEntry {
        mutable Mutex    mutex;
        mutable ThreadEx threadEx;
        Peer             peer;
        NoticeQueue      noticeQueue;
        QueueIndex       readIndex;
        Thread           thread;

        void run(const bool pubPath) {
            LOG_TRACE;
            try {
                /*
                 * Starting the peer here, on a separate thread, means that it
                 * won't block other peers if it was client-side constructed and
                 * has yet to connect to the remote peer.
                 */
                peer.start(); // Starts serving the remote peer
                peer.notify(PubPath(pubPath));

                for(;;) {
                    /*
                     * Because the following blocks indefinitely, the current
                     * thread must be cancelled in order to stop it and this
                     * must be done before this instance is destroyed.
                     */
                    noticeQueue.send(readIndex, peer);
                    /*
                     * To avoid prematurely purging the current PDU, the
                     * read-index must be incremented *after* the notice has
                     * been sent.
                     */
                    ++readIndex;
                }
            }
            catch (const std::exception& ex) {
                threadEx.set(ex);
            }
        }

    public:
        PeerEntry(Peer        peer,
                  NoticeQueue noticeQueue,
                  const bool  pubPath)
            : mutex()
            , threadEx()
            , peer(peer)
            , noticeQueue(noticeQueue)
            , readIndex(noticeQueue.getOldestIndex())
            , thread(&PeerEntry::run, this, pubPath)
        {}

        PeerEntry(const PeerEntry& peerEntry) =delete;
        PeerEntry& operator=(const PeerEntry& entry) =delete;

        PeerEntry(PeerEntry&& peerEntry) =default;

        ~PeerEntry() {
            /*
             * Canceling the thread should be safe because it's only sending
             * notifications to the remote peer.
             */
            ::pthread_cancel(thread.native_handle());
            thread.join();

            peer.stop();
        }

        QueueIndex getReadIndex() const {
            Guard guard(mutex);
            threadEx.throwIfSet();
            return readIndex;
        }
    };

    using PeerEntries = std::map<Peer, PeerEntry>;

    mutable Mutex mutex;
    NoticeQueue   noticeQueue;
    // Placed after notice queue to ensure existence for `PeerEntry.run()`
    PeerEntries   peerEntries;

    /**
     * Purges notification queues of entries that will no longer be read.
     *
     * @pre `mutex` is locked
     * @pre `!peerEntries.empty()`
     */
    void purge() {
        LOG_ASSERT(!mutex.try_lock());
        LOG_ASSERT(!peerEntries.empty());

        const auto writeIndex = noticeQueue.getWriteIndex();
        // Guaranteed to be no older than oldest read-index:
        auto       oldestIndex = writeIndex;

        // Find oldest read-index
        for (const auto& peerEntry : peerEntries) {
            const auto readIndex = peerEntry.second.getReadIndex();

            if (readIndex < oldestIndex)
                oldestIndex = readIndex;
        }

        if (oldestIndex < writeIndex) {
            // Purge notice queue of entries that will no longer be read
            noticeQueue.eraseTo(oldestIndex);
        }
    }

public:
    Impl(P2pNode& node)
        : mutex()
        , noticeQueue(node)
        , peerEntries()
    {}

    /**
     * Adds a peer to this instance and starts it iff the peer is not already in
     * the set.
     *
     * @param[in] peer           Peer to be added
     * @param[in] pubPath        Is local peer path to publisher?
     * @retval    `false`        Peer was not added because it already exists
     * @retval    `true`         Peer was added
     * @throw std::system_error  Couldn't create new thread
     */
    bool insert(Peer peer, const bool pubPath) {
        Guard guard(mutex);
        bool  added;

        if (peerEntries.count(peer)) {
            added = false;
        }
        else {
            // NB: The following requires that `peer.hash()` works now
            const auto  pair = peerEntries.emplace(std::piecewise_construct,
                    std::forward_as_tuple(peer),
                    std::forward_as_tuple(peer, noticeQueue, pubPath));

            LOG_ASSERT(pair.second); // Because `peerEntries.count(peer) != 0`

            added = true;
        }

        return added;
    }

    bool erase(Peer peer) {
        Guard guard(mutex);
        return peerEntries.erase(peer);
    }

    PeerEntries::size_type size() const {
        Guard guard(mutex);
        return peerEntries.size();
    }

    void notify(const PubPath notice) {
        Guard guard(mutex);
        if (!peerEntries.empty()) {
            noticeQueue.putPubPath(notice);
            purge();
        }
    }

    void notify(const ProdIndex notice) {
        Guard guard(mutex);
        if (!peerEntries.empty()) {
            noticeQueue.putProdIndex(notice);
            purge();
        }
    }

    void notify(const DataSegId& notice) {
        Guard guard(mutex);
        if (!peerEntries.empty()) {
            noticeQueue.put(notice);
            purge();
        }
    }
};

/******************************************************************************/

PeerSet::PeerSet(P2pNode& node)
    : pImpl{std::make_shared<Impl>(node)}
{}

bool PeerSet::insert(Peer peer, const bool pubPath) const {
    return pImpl->insert(peer, pubPath);
}

bool PeerSet::erase(Peer peer) const {
    return pImpl->erase(peer);
}

PeerSet::size_type PeerSet::size() const {
    return pImpl->size();
}

void PeerSet::notify(const PubPath notice) const {
    pImpl->notify(notice);
}

void PeerSet::notify(const ProdIndex notice) const {
    pImpl->notify(notice);
}

void PeerSet::notify(const DataSegId& notice) const {
    pImpl->notify(notice);
}

} // namespace
