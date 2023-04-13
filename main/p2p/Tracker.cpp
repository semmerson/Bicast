/**
 * Thread-safe pool of addresses of peer servers.
 *
 *        File: Tracker.cpp
 *  Created on: Jun 29, 2019
 *      Author: Steven R. Emmerson
 *
 *    Copyright 2022 University Corporation for Atmospheric Research
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
#include "Tracker.h"
#include "Xprt.h"

#include <chrono>
#include <iterator>
#include <unordered_map>
#include <utility>

namespace hycast {

/// Tracker of P2P-servers
class Tracker::Impl final : public XprtAble
{
    struct Links {
        SockAddr prev;
        SockAddr next;
        Links(  const SockAddr prev,
                const SockAddr next)
            : prev(prev)
            , next(next)
        {}
    };
    using Map  = std::unordered_map<SockAddr, Links>;
    using Size = uint32_t;

    mutable Mutex mutex;
    mutable Cond  cond;
    Size          capacity;
    Map           map;
    SockAddr      head;
    SockAddr      tail;
    bool          done;

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
     * Removes an element.
     * @param[in] srvrAddr  Socket address of the element to be removed
     */
    void remove(const SockAddr& srvrAddr) {
        LOG_ASSERT(!mutex.try_lock());

        auto iter = map.find(srvrAddr);
        if (iter != map.end()) {
            const auto& links = iter->second;

            if (links.prev)
                map.at(links.prev).next = links.next;
            else
                head = links.next;

            if (links.next)
                map.at(links.next).prev = links.prev;
            else
                tail = links.prev;

            map.erase(iter);
        }
    }

    /**
     * Adds a socket address to the back. If the capacity is exceeded, then the front entry is
     * deleted.
     *
     * @pre                    Mutex is locked
     * @param[in] srvrAddr     Socket address to be added
     * @retval    true         Address was added
     * @retval    false        Address was not added because it already exists
     * @throw InvalidArgument  Socket address is invalid
     */
    bool addBack(const SockAddr& srvrAddr) {
        LOG_ASSERT(!mutex.try_lock());

        if (!srvrAddr)
            throw INVALID_ARGUMENT("Socket address is invalid");

        if (!map.emplace(srvrAddr, Links(tail, SockAddr{})).second)
            return false;

        if (tail)
            map.at(tail).next = srvrAddr;

        tail = srvrAddr;
        if (!head)
            head = srvrAddr;

        if (map.size() > capacity)
            remove(head);

        cond.notify_all();
        return true;
    }

    /**
     * Adds a socket address to the front. If the capacity is exceeded, then the back entry is
     * deleted.
     *
     * @pre                    Mutex is locked
     * @param[in] srvrAddr     Socket address to be added
     * @retval    true         Address was added
     * @retval    false        Address was not added because it already exists
     * @throw InvalidArgument  Socket address is invalid
     */
    bool addFront(const SockAddr& srvrAddr) {
        LOG_ASSERT(!mutex.try_lock());

        if (!srvrAddr)
            throw INVALID_ARGUMENT("Socket address is invalid");

        if (!map.emplace(srvrAddr, Links(SockAddr{}, head)).second)
            return false;

        if (head)
            map.at(head).prev = srvrAddr;

        head = srvrAddr;
        if (!tail)
            tail = srvrAddr;

        if (map.size() > capacity)
            remove(tail);

        cond.notify_all();
        return true;
    }

public:
    /**
     * Constructs.
     * @param[in] capacity  Maximum number of entries
     */
    Impl(const size_t capacity)
        : mutex()
        , cond()
        , capacity(capacity)
        , map()
        , done(false)
    {
        if (capacity == 0)
            throw INVALID_ARGUMENT("Capacity is zero");
    }

    /**
     * Returns the string representation of this instance.
     * @return The string representation of this instance
     */
    std::string to_string() const {
        Guard guard(mutex);
        return "{size=" + std::to_string(map.size()) + "}";
    }

    /**
     * Returns the number of P2P server addresses.
     * @return The number of P2P server addresses
     */
    size_t size() const {
        Guard guard(mutex);
        return map.size();
    }

    /**
     * Adds the socket address of a P2P server to the back.
     * @param[in] srvrAddr  The P2P server's address to be added
     * @retval    true      Success
     * @retval    false     The P2P server's address already exists
     */
    bool insertBack(const SockAddr& srvrAddr) {
        Guard guard(mutex);
        return addBack(srvrAddr);
    }

    /**
     * Adds the socket addresses of P2P servers contained in another instance to the front. The
     * order of entries in the other instance is maintained. If the capacity is exceeded, then
     * entries at the back are deleted.
     * @param[in] src  The other instance
     */
    void insertFront(const Impl& src) {
        if (this != &src) { // No need if same object
            Mutex* mutex1;
            Mutex* mutex2;
            orderMutexes(src, &mutex1, &mutex2);
            Guard guard1(*mutex1);
            Guard guard2(*mutex2);

            for (auto srvrAddr = src.tail; srvrAddr; srvrAddr = src.map.at(srvrAddr).prev)
                addFront(srvrAddr);
        }
    }

    /**
     * Deletes the socket address of a P2P server.
     * @param[in] srvrAddr  The socket address of the P2P server to delete
     */
    void erase(const SockAddr srvrAddr) {
        Guard guard(mutex);
        return remove(srvrAddr);
    }

    /**
     * Deletes all P2P server socket addresses contained in an instance.
     * @param[in] src  The instance
     */
    void erase(const Impl& src) {
        if (this == &src) {
            map.clear();
        }
        else {
            Mutex* mutex1;
            Mutex* mutex2;
            orderMutexes(src, &mutex1, &mutex2);
            Guard guard1(*mutex1);
            Guard guard2(*mutex2);
            for (const auto& pair : src.map)
                remove(pair.first);
        }
    }

    /**
     * Removes and returns the first P2P server's socket address. Blocks until one is available or
     * `halt()` is called.
     * @return The first P2P server's socket address. Will test false if `halt()` has been called.
     * @see halt()
     */
    SockAddr removeFront() {
        Lock lock{mutex};
        cond.wait(lock, [&]{return !map.empty() || done;});

        if (done)
            return SockAddr();

        auto newHead = map.at(head).next;
        if (newHead) {
            map.at(newHead).prev = SockAddr();
        }
        else {
            tail = SockAddr();
        }
        map.erase(head);
        auto oldHead = head;
        head = newHead;
        return oldHead;
    }

    /**
     * Causes `removeHead()` to always return an invalid value.
     */
    void halt() {
        Guard guard{mutex};
        done = true;
        cond.notify_all();
    }

    bool write(Xprt xprt) const {
        Guard guard{mutex};
        auto size = static_cast<Size>(map.size());
        LOG_DEBUG("Writing size=%s", std::to_string(size).data());
        if (!xprt.write(size))
            return false;
        for (auto srvrAddr = head; srvrAddr; srvrAddr = map.at(srvrAddr).next) {
            LOG_DEBUG("Writing socket address %s", srvrAddr.to_string().data());
            if (!srvrAddr.write(xprt))
                return false;
        }
        return true;
    }

    bool read(Xprt xprt) {
        Guard guard{mutex};
        Size size;
        if (!xprt.read(size))
            return false;
        LOG_DEBUG("Read size=%s", std::to_string(size).data());
        map.clear();
        for (Size i = 0; i < size; ++i) {
            SockAddr srvrAddr;
            if (!srvrAddr.read(xprt))
                return false;
            LOG_DEBUG("Read socket address %s", srvrAddr.to_string().data());
            addBack(srvrAddr);
        }
        cond.notify_all();
        return true;
    }
};

// TODO: Make capacity user-configurable
Tracker::Tracker(const size_t capacity)
    : pImpl{new Impl(capacity)} {
}

std::string Tracker::to_string() const {
    return pImpl->to_string();
}

size_t Tracker::size() const {
    return pImpl->size();
}

bool Tracker::insertBack(const SockAddr& peerSrvrAddr) const {
    return pImpl->insertBack(peerSrvrAddr);
}

void Tracker::insertFront(const Tracker tracker) const {
    pImpl->insertFront(*tracker.pImpl);
}

void Tracker::erase(const SockAddr srvrAddr) {
    pImpl->erase(srvrAddr);
}

void Tracker::erase(const Tracker tracker) {
    pImpl->erase(*tracker.pImpl);
}

SockAddr Tracker::removeFront() const {
    return pImpl->removeFront();
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
