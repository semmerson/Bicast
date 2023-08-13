/**
 * @file: Tracker.cpp
 *
 * Thread-safe pool of addresses of peer servers.
 *
 *  Created on: Jun 29, 2019
 *      Author: Steven R. Emmerson
 *
 *    Copyright 2023 University Corporation for Atmospheric Research
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

#include "error.h"
#include "HycastProto.h"
#include "ThreadException.h"
#include "Xprt.h"

#include <chrono>
#include <iterator>
#include <map>
#include <queue>
#include <set>
#include <unordered_map>
#include <utility>

namespace hycast {

/// Tracker of P2P-servers
class Tracker::Impl final : public XprtAble
{
    struct SrvrElt {
        P2pSrvrInfo info;
        enum class State {
            AVAILABLE,
            ACTIVE,
            DELAYED
        }           state;
    };
    using SrvrInfoMap = std::unordered_map<SockAddr, P2pSrvrInfo>;
    SrvrInfoMap srvrInfos; /// Information on presumably valid P2P servers

    class QueueComp {
        Impl& trackerImpl;

    public:
        QueueComp(Tracker::Impl& trackerImpl)
            : trackerImpl{trackerImpl}
        {}

        /**
         * Indicates if one server's address is less than another.
         * @param[in] srvr1   Socket address of one server
         * @param[in] srvr2   Socket address of another server
         * @retval true       The first argument is less than the second
         * @retval false      The first argument is not less than the second
         */
        bool operator()(
                const SockAddr& srvr1,
                const SockAddr& srvr2) const {
            LOG_ASSERT(trackerImpl.srvrInfos.count(srvr1));
            LOG_ASSERT(trackerImpl.srvrInfos.count(srvr2));

            const auto& info1 = trackerImpl.srvrInfos.at(srvr1);
            const auto& info2 = trackerImpl.srvrInfos.at(srvr2);

            // Server for which no metrics exist is greatest
            if ((!info1.validMetrics() || !info2.validMetrics()) &&
                    info1.validMetrics() != info2.validMetrics())
                return info1.validMetrics();

            if (info1.tier != info2.tier) {
                return info1.tier < info2.tier; // Closer to source is less
            }
            else if (info1.numAvail != info2.numAvail) {
                return info1.numAvail > info2.numAvail; // Greater availability is less
            }
            else if (info1.valid != info2.valid) {
                return info1.valid > info2.valid; // Newer information is less
            }
            else {
                return srvr1 < srvr2; // Intrinsic sort-order of server's socket address
            }
        }
    };

    using SrvrQueue = std::set<SockAddr, QueueComp>;
    SrvrQueue srvrQueue;

    struct DelayElt {
        SockAddr     srvrAddr;
        SysTimePoint revealTime;
        DelayElt() =default;
        DelayElt(
                const SockAddr&     srvrAddr,
                const SysTimePoint& revealTime)
            : srvrAddr(srvrAddr)
            , revealTime(revealTime)
        {}
        bool operator<(const DelayElt& elt) const {
            return revealTime > elt.revealTime; // Later reveal time has lower priority
        }
    };
    using DelayQueue = std::priority_queue<DelayElt>;
    DelayQueue       delayQueue;

    using Size       = uint32_t;  ///< Size type for serialization

    mutable Mutex    mutex;       ///< Protects state changes
    mutable Cond     queueCond;   ///< For waiting on changes to the server queue
    Size             capacity;    ///< Maximum number of servers to hold
    bool             done;        ///< Is this instance done?
    SysDuration      delay;       ///< Delay for a server before re-making it available
    mutable Cond     delayCond;   ///< For waiting on changes to the delay queue
    mutable ThreadEx threadEx;    ///< Exception thrown by internal thread
    Thread           delayThread; ///< Thread for moving servers from delay queue to server queue

#ifdef NDEBUG
    inline void assertInvariant() {
    }
#else
    void assertInvariant() {
        LOG_ASSERT(!mutex.try_lock());

        for (auto& srvrAddr : srvrQueue)
            LOG_ASSERT(srvrInfos.count(srvrAddr));
    }
#endif

    /**
     * Erases a P2P server from the database.
     * @param[in] srvrAddr  Socket address of the P2P server
     */
    inline void erase(const SockAddr& srvrAddr) {
        srvrQueue.erase(srvrAddr);
        srvrInfos.erase(srvrAddr);
    }

    /**
     * Updates the database for a P2P server. Creates an entry if necessary.
     * @param[in] srvrInfo  Information on a P2P server
     */
    void update(const P2pSrvrInfo& srvrInfo) {
        const auto& srvrAddr = srvrInfo.srvrAddr;

        erase(srvrAddr);

        srvrInfos[srvrAddr] = srvrInfo;
        srvrQueue.insert(srvrAddr);
    }

    /**
     * Moves P2P-servers from the delay queue to the server queue. Implemented as a start routine
     * for a thread that expects `halt()` to be called.
     * @see peerDisconnected()
     * @see halt()
     */
    void runDelayQueue() {
        try {
            auto pred = [&] {return delayQueue.top().revealTime <= SysClock::now() || done;};
            Lock lock{mutex};

            for (;;) {
                // Wait until the delay-queue is not empty
                if (delayQueue.empty()) {
                    delayCond.wait(lock, [&]{return !delayQueue.empty() || done;});
                    if (done)
                        break;
                }
                /*
                 * and
                 *   - The reveal-time for the top entry has expired; or
                 *   - `halt()` has been called.
                 * NB: Two condition variable waits are used because
                 *   1) The alternative predicate "(!delayQueue.empty() &&
                 *      delayQueue.top().revealTime <= " "SysClock::now()) || done" would require
                 *      that the condition variable be notified in order to return the next element
                 *      -- regardless of it's reveal-time; and
                 *   2) `delayCond.wait_until()` can't be used on an empty queue.
                 */
                delayCond.wait_until(lock, delayQueue.top().revealTime, pred);
                if (done)
                    break;

                auto& srvrAddr = delayQueue.top().srvrAddr; // NB: reference
                if (srvrInfos.count(srvrAddr)) {
                    // Information on server hasn't been removed -- so it's a valid candidate
                    // Erasure & insertion == update
                    srvrQueue.erase(srvrAddr);
                    srvrQueue.insert(srvrAddr);
                    queueCond.notify_all();
                }
                delayQueue.pop(); // Must come after use of `srvrAddr` if it's a reference
            }
        }
        catch (const std::exception& ex) {
            threadEx.set(std::current_exception());
        }
    }

    /**
     * Determines the order in which the mutex in this instance and another instance should be
     * locked in order to prevent deadlock.
     */
    void orderMutexes(
            const Impl&   that,
            Mutex** const mutex1,
            Mutex** const mutex2) {
        if (this < &that) {
            *mutex1 = &mutex;
            *mutex2 = &that.mutex;
        }
        else {
            *mutex2 = &mutex;
            *mutex1 = &that.mutex;
        }
    }

    /**
     * Tries to insert information on a P2P-server. If an entry for the server doesn't exist, then
     * it is created; otherwise, the existing information is updated if the given information is
     * more recent. If the capacity is exceeded, then the worst entry is deleted.
     *
     * @param[in] srvrInfo  Information on the P2P-server
     * @retval    true      Success. New information inserted or updated.
     * @retval    false     More recent server information exists. No insertion.
     * @exceptionsafety     Strong guarantee
     * @threadsafety        Safe
     */
    bool tryAdd(const P2pSrvrInfo& srvrInfo) {
        LOG_ASSERT(srvrInfo);
        LOG_ASSERT(!mutex.try_lock());

        bool success;
        auto iter = srvrInfos.find(srvrInfo.srvrAddr);

        if (iter == srvrInfos.end()) {
            // Server is not known
            if (srvrInfos.size() >= capacity) {
                // At capacity. Delete worst server.
                LOG_ASSERT(!srvrQueue.empty());
                const auto worstSrvrAddr = *srvrQueue.rbegin();
                erase(worstSrvrAddr);
            }

            update(srvrInfo);
            success = true;
        }
        else {
            // Server is known
            success = srvrInfo.valid > iter->second.valid;
            if (success) {
                // Given information is more recent => update
                if (!srvrQueue.count(srvrInfo.srvrAddr)) {
                    // Not in server-queue, so just update information
                    iter->second = srvrInfo;
                }
                else {
                    // Update entry
                    update(srvrInfo);
                }
            }
        }

        assertInvariant();

        return success;
    }

public:
    /**
     * Constructs.
     * @param[in] capacity  Maximum number of entries
     * @param[in] delay     Delay for a server before re-making it available
     */
    Impl(   const size_t       capacity,
            const SysDuration& delay)
        : srvrInfos(capacity)
        , srvrQueue(QueueComp(*this))
        , delayQueue()
        , mutex()
        , queueCond()
        , capacity(capacity)
        , done(false)
        , delay(delay)
        , delayCond()
        , threadEx()
        , delayThread(&Impl::runDelayQueue, this)
    {
        if (capacity == 0)
            throw INVALID_ARGUMENT("Capacity is zero");
    }

    ~Impl() noexcept {
        halt();
        delayThread.join();
    }

    /**
     * Returns a string representation.
     * @return A string representation
     */
    std::string to_string() const {
        Guard guard(mutex);
        return "{capacity=" + std::to_string(capacity) + ", size=" +
                std::to_string(srvrInfos.size()) + "}";
    }

    /**
     * Returns the number of known P2P-servers.
     * @return The number of known P2P-servers
     */
    size_t size() const {
        Guard guard(mutex);
        return srvrInfos.size();
    }

    /**
     * Tries to insert information on a P2P-server. If the server's address doesn't exist, then the
     * information is inserted; otherwise, the existing information is updated if the given
     * information is better. If the capacity is exceeded, then the worst entry is deleted.
     *
     * @param[in] srvrInfo  Information on a P2P-server
     * @retval    true      Success. New information inserted or updated.
     * @retval    false     More recent server information exists. No insertion.
     * @exceptionsafety     Strong guarantee
     * @threadsafety        Safe
     */
    bool insert(const P2pSrvrInfo& srvrInfo) {
        threadEx.throwIfSet();

        Guard guard(mutex);
        return tryAdd(srvrInfo);
    }

    /**
     * Inserts the entries from another instance.
     * @param[in] tracker  The other instance
     */
    void insert(const Impl& tracker) {
        threadEx.throwIfSet();

        if (this != &tracker) { // No need if same object
            Mutex* mutex1;
            Mutex* mutex2;
            orderMutexes(tracker, &mutex1, &mutex2);
            Guard guard1(*mutex1);
            Guard guard2(*mutex2);

            for (auto& pair : tracker.srvrInfos)
                tryAdd(pair.second);
        }
    }

    /**
     * Removes and returns the address of the next P2P-server to try. Blocks until one is available
     * or `halt()` has been called.
     * @return The address of the next P2P-server to try. Will test false if `halt()` has been
     *         called.
     * @see halt()
     */
    SockAddr getNextAddr() {
        threadEx.throwIfSet();

        Lock lock{mutex};
        queueCond.wait(lock, [&]{return !srvrQueue.empty() || done;});

        if (done)
            return SockAddr();

        auto       iter = srvrQueue.begin();
        const auto srvrAddr = *iter;

        srvrQueue.erase(iter);

        assertInvariant();

        return srvrAddr;
    }

    /**
     * Handles a P2P-server that's offline.
     * @param[in] p2pSrvrAddr  Socket address of the remote P2P-server
     */
    void offline(const SockAddr& p2pSrvrAddr) {
        threadEx.throwIfSet();

        Guard guard{mutex};

        erase(p2pSrvrAddr);
    }

    /**
     * Handles a peer disconnecting. If the local peer was constructed client-side, then the remote
     * P2P-server has its number of available server-side peers increased.
     * @param[in] srvrAddr     Socket address of the remote P2P-server
     * @param[in] wasClient    Was the local peer constructed client-side?
     * @retval true            P2P-server is known
     * @retval false           P2P-server is not known. Nothing was done.
     */
    bool disconnected(
            const SockAddr& srvrAddr,
            const bool      wasClient) {
        threadEx.throwIfSet();

        Guard guard{mutex};

        if (srvrInfos.count(srvrAddr) == 0)
            return false;

        srvrQueue.erase(srvrAddr);

        auto& srvrInfo = srvrInfos[srvrAddr];
        if (wasClient)
            srvrInfo.numAvail = srvrInfo.validMetrics() ? srvrInfo.numAvail + 1 : 1;
        srvrInfo.valid = SysClock::now();

        delayQueue.emplace(srvrAddr, srvrInfo.valid + delay);
        delayCond.notify_all();

        return true;
    }

    /**
     * Causes `getNextAddr()` to always return a socket address that tests false. Idempotent.
     */
    void halt() {
        Guard guard{mutex};
        done = true;
        queueCond.notify_all();
        delayCond.notify_all();
    }

    /**
     * Writes itself to a transport.
     * @param[in] xprt     The transport
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    bool write(Xprt xprt) const {
        threadEx.throwIfSet();

        Guard guard{mutex};

        auto size = static_cast<Size>(srvrInfos.size());
        LOG_TRACE("Writing size=%s", std::to_string(size).data());
        if (!xprt.write(size))
            return false;

        for (auto& pair : srvrInfos)
            if (!pair.second.write(xprt))
                return false;

        return true;
    }

    /**
     * Reads itself from a transport.
     * @param[in] xprt     The transport
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    bool read(Xprt xprt) {
        threadEx.throwIfSet();

        Guard guard{mutex};

        Size size;
        if (!xprt.read(size))
            return false;
        LOG_TRACE("Read size=%s", std::to_string(size).data());

        while (!delayQueue.empty())
            delayQueue.pop();
        srvrQueue.clear();
        srvrInfos.clear();

        for (Size i = 0; i < size; ++i) {
            P2pSrvrInfo srvrInfo;

            if (!srvrInfo.read(xprt))
                return false;
            LOG_DEBUG("Read tracker entry " + srvrInfo.to_string());

            tryAdd(srvrInfo);
        }

        return true;
    }
};

Tracker::Tracker(
        const size_t       capacity,
        const SysDuration& delay)
    : pImpl{new Impl(capacity, delay)} {
}

std::string Tracker::to_string() const {
    return pImpl->to_string();
}

size_t Tracker::size() const {
    return pImpl->size();
}

bool Tracker::insert(const P2pSrvrInfo& srvrInfo) const {
    return pImpl->insert(srvrInfo);
}

void Tracker::insert(const Tracker tracker) const {
    pImpl->insert(*tracker.pImpl);
}

SockAddr Tracker::getNextAddr() const {
    return pImpl->getNextAddr();
}

void Tracker::offline(const SockAddr p2pSrvrAddr) const {
    pImpl->offline(p2pSrvrAddr);
}

bool Tracker::disconnected(
        const SockAddr p2pSrvrAddr,
        const bool     wasClient) const {
    return pImpl->disconnected(p2pSrvrAddr, wasClient);
}

void Tracker::halt() const {
    pImpl->halt();
}

bool Tracker::write(Xprt xprt) const {
    return pImpl->write(xprt);
}

bool Tracker::read(Xprt xprt) {
    return pImpl->read(xprt);
}

} // namespace
