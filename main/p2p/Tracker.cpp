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
    SrvrInfoMap srvrInfos;

    class QueueComp {
        Impl& trackerImpl;

    public:
        QueueComp(Tracker::Impl& trackerImpl)
            : trackerImpl{trackerImpl}
        {}

        /**
         * Indicates if one server is less than another.
         * @param[in] srvr1   Socket address of one server
         * @param[in] srvr2   Socket address of another server
         * @retval true       The first argument is less than the second
         * @retval false      The first argument is not less than the second
         * @throw LogicError  The socket address are identical
         */
        bool operator()(
                const SockAddr& srvr1,
                const SockAddr& srvr2) const {
            if (srvr1 == srvr2)
                throw LOGIC_ERROR("Identical socket addresses");

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
    SrvrQueue   srvrQueue;

    using SockAddrSet = std::unordered_set<SockAddr>;
    SockAddrSet activeSrvrs;

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
    inline void assertInvariant(const SockAddr& srvrAddr) {
    }
#else
    void assertInvariant(const SockAddr& srvrAddr) {
        LOG_ASSERT(!mutex.try_lock());


        const unsigned status =
                (srvrInfos.count(srvrAddr) & 0x4) |
                (srvrQueue.count(srvrAddr) & 0x2) |
                (activeSrvrs.count(srvrAddr) & 0x1);

        LOG_ASSERT(status == 0x6 || status == 0x5 || status == 4 || status == 0);
        LOG_ASSERT(srvrInfos.size() == srvrQueue.size() + activeSrvrs.size() + delayQueue.size());
    }
#endif

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
                 */
                delayCond.wait_until(lock, delayQueue.top().revealTime, pred);
                if (done)
                    break;

                srvrQueue.insert(delayQueue.top().srvrAddr);
                delayQueue.pop();
                queueCond.notify_all();
            }
        }
        catch (const std::exception& ex) {
            threadEx.set(ex);
        }
    }

    /**
     * Ensures that mutexes are always locked in the same order to prevent deadlock.
     */
    void orderMutexes(
            const Impl& that,
            Mutex**     mutex1,
            Mutex**     mutex2) {
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
     * Removes a server.
     * @param[in] srvrAddr  Socket address of the server to be removed
     */
    void remove(const SockAddr srvrAddr) {
        LOG_ASSERT(!mutex.try_lock());

        assertInvariant(srvrAddr);

        srvrInfos.erase(srvrAddr);
        srvrQueue.erase(srvrAddr);
        activeSrvrs.erase(srvrAddr);
    }

    /**
     * Tries to insert information on a P2P-server. If an entry for the server doesn't exist, then
     * the information is inserted; otherwise, the existing information is updated if the given
     * information is better. If the capacity is exceeded, then the worst entry is deleted.
     *
     * @param[in] srvrInfo  Information on the P2P-server
     * @retval    true      Success. New information inserted or updated.
     * @retval    false     More recent server information exists. No insertion.
     * @exceptionsafety     Strong guarantee
     * @threadsafety        Safe
     */
    bool tryAdd(const P2pSrvrInfo& srvrInfo) {
        LOG_ASSERT(!mutex.try_lock());

        bool success;
        auto iter = srvrInfos.find(srvrInfo.srvrAddr);

        if (iter == srvrInfos.end()) {
            // Server is not known
            if (srvrInfos.size() >= capacity)
                remove(*srvrQueue.rbegin()); // At capacity. Delete worst server.

            srvrInfos[srvrInfo.srvrAddr] = srvrInfo; // Must be done before inserting into queue
            srvrQueue.insert(srvrInfo.srvrAddr);
            success = true;
        }
        else {
            // Server is known
            success = srvrInfo.valid > iter->second.valid;
            if (success) {
                // Given information is more recent => update
                iter->second = srvrInfo; // Must be done before updating queue
                // Update queue
                srvrQueue.erase(srvrInfo.srvrAddr);
                srvrQueue.insert(srvrInfo.srvrAddr);
            }
        }

        assertInvariant(srvrInfo.srvrAddr);

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
        , activeSrvrs()
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

    /**
     * Destroys.
     */
    ~Impl() noexcept {
        halt();
        delayThread.join();
    }

    /**
     * Returns the string representation of this instance.
     * @return The string representation of this instance
     */
    std::string to_string() const {
        Guard guard(mutex);
        return "{capacity=" + std::to_string(capacity) + ", size=" +
                std::to_string(srvrInfos.size()) + ", active=" + std::to_string(activeSrvrs.size())
                + "}";
    }

    /**
     * Returns the number of P2P-server addresses.
     * @return The number of P2P-server addresses
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
     * Tries to adds information on P2P-servers contained in another instance.
     * @param[in] src  The other instance
     */
    void insert(const Impl& src) {
        threadEx.throwIfSet();

        if (this != &src) { // No need if same object
            Mutex* mutex1;
            Mutex* mutex2;
            orderMutexes(src, &mutex1, &mutex2);
            Guard guard1(*mutex1);
            Guard guard2(*mutex2);

            for (auto& pair : src.srvrInfos)
                tryAdd(pair.second);
        }
    }

    /**
     * Deletes the entry for a P2P-server.
     * @param[in] srvrAddr  The socket address of the P2P-server to delete
     */
    void erase(const SockAddr srvrAddr) {
        threadEx.throwIfSet();

        Guard guard(mutex);
        return remove(srvrAddr);
    }

    /**
     * Deletes all P2P-server entries contained in another instance. Does not, however, erase
     * servers returned by `getP2pSrvr()`.
     * @param[in] src  The other instance with the servers that should be erased from this instance
     */
    void erase(const Impl& src) {
        threadEx.throwIfSet();

        if (this == &src) {
            Guard guard{mutex};

            for (auto& srvrAddr : srvrQueue)
                srvrInfos.erase(srvrAddr);

            srvrQueue.clear();
        }
        else {
            Mutex* mutex1;
            Mutex* mutex2;
            orderMutexes(src, &mutex1, &mutex2);
            Guard guard1(*mutex1);
            Guard guard2(*mutex2);

            for (auto& srvrAddr : src.srvrQueue) {
                srvrInfos.erase(srvrAddr);
                srvrQueue.erase(srvrAddr);
            }
        }
    }

    /**
     * Returns the socket address of the next P2P-server to try. Blocks until one is available or
     * `halt()` is called.
     * @return The P2P-server's socket address. Will test false if `halt()` has been called.
     * @see halt()
     */
    SockAddr getNext() {
        threadEx.throwIfSet();

        Lock lock{mutex};
        queueCond.wait(lock, [&]{return !srvrQueue.empty() || done;});

        if (done)
            return SockAddr();

        auto       iter = srvrQueue.begin();
        const auto srvrAddr = *iter;
        activeSrvrs.insert(srvrAddr);

        srvrQueue.erase(iter);

        assertInvariant(srvrAddr);

        return srvrAddr;
    }

    /**
     * Handles a remote P2P-server going offline. The server will be deleted and no longer made
     * available.
     * @param[in] srvrAddr  Socket address of remote P2P-server
     */
    void offline(const SockAddr& srvrAddr) {
        threadEx.throwIfSet();

        Guard guard{mutex};

        LOG_ASSERT(srvrInfos.count(srvrAddr));
        LOG_ASSERT(activeSrvrs.count(srvrAddr));
        LOG_ASSERT(srvrQueue.count(srvrAddr) == 0);

        remove(srvrAddr);
    }

    /**
     * Handles the disconnection of a peer whose remote address was returned by `getNext()`. The
     * server will be delayed before it's made available.
     * @param[in] srvrAddr  Socket address of associated remote P2P-server
     * @see getNext()`
     * @see runDelayQueue()
     */
    void disconnected(const SockAddr& srvrAddr) {
        threadEx.throwIfSet();

        Guard guard{mutex};

        LOG_ASSERT(srvrInfos.count(srvrAddr));
        LOG_ASSERT(activeSrvrs.count(srvrAddr));
        LOG_ASSERT(srvrQueue.count(srvrAddr) == 0);

        activeSrvrs.erase(srvrAddr);

        const auto now = SysClock::now();
        srvrInfos.at(srvrAddr).valid = now;
        delayQueue.emplace(srvrAddr, now + delay);

        delayCond.notify_all();
    }

    /**
     * Causes `getP2pSrvr()` to always return an invalid value.
     */
    void halt() {
        Guard guard{mutex};
        done = true;
        queueCond.notify_all();
        delayCond.notify_all();
    }

    bool write(Xprt xprt) const {
        threadEx.throwIfSet();

        Guard guard{mutex};

        auto size = static_cast<Size>(srvrInfos.size());
        LOG_DEBUG("Writing size=%s", std::to_string(size).data());
        if (!xprt.write(size))
            return false;

        for (auto& pair : srvrInfos)
            if (!pair.second.write(xprt))
                return false;

        return true;
    }

    bool read(Xprt xprt) {
        threadEx.throwIfSet();

        Guard guard{mutex};

        Size size;
        if (!xprt.read(size))
            return false;
        LOG_DEBUG("Read size=%s", std::to_string(size).data());

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

// TODO: Make capacity user-configurable
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

void Tracker::erase(const SockAddr srvrAddr) {
    pImpl->erase(srvrAddr);
}

void Tracker::erase(const Tracker tracker) {
    pImpl->erase(*tracker.pImpl);
}

SockAddr Tracker::getNextAddr() const {
    return pImpl->getNext();
}

void Tracker::offline(const SockAddr p2pSrvrAddr) const {
    pImpl->offline(p2pSrvrAddr);
}

void Tracker::disconnected(const SockAddr p2pSrvrAddr) const {
    pImpl->disconnected(p2pSrvrAddr);
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
