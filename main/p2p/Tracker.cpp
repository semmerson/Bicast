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
        Links(const SockAddr tail)
            : prev(tail)
            , next()
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

    /**
     * Adds a socket address.
     *
     * @pre                 Mutex is locked
     * @param[in] srvrAddr  Socket address to be added
     * @retval    `true`    Address was added
     * @retval    `false`   Address was not added because it already exists
     */
    bool add(const SockAddr& srvrAddr) {
        LOG_ASSERT(!mutex.try_lock());

        auto pair = map.emplace(srvrAddr, Links(tail));
        if (!pair.second)
            return false;

        tail = srvrAddr;
        if (!head) {
            head = srvrAddr;
        }
        else if (map.size() > capacity) {
            // `capacity >= 1` => there must be at least two entries
            auto newHead = map.at(head).next;
            map.erase(head);
            head = newHead;
            map.at(newHead).prev = SockAddr();
        }

        cond.notify_all();
        return true;
    }

    void remove(const SockAddr& srvrAddr) {
        Guard guard(mutex);
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

public:
    Impl(const size_t capacity)
        : mutex()
        , cond()
        , capacity(capacity)
        , map()
    {
        if (capacity == 0)
            throw INVALID_ARGUMENT("Capacity is zero");
    }

    std::string to_string() const {
        Guard guard(mutex);
        return "{size=" + std::to_string(map.size()) + "}";
    }

    size_t size() const {
        Guard guard(mutex);
        return map.size();
    }

    bool insert(const SockAddr& srvrAddr) {
        Guard guard(mutex);
        return add(srvrAddr);
    }

    void insert(const Impl& src) {
        if (this != &src) { // No need if same object
            // Ensure that mutexes are always locked in the same order to prevent deadlock
            Mutex* mutex1;
            Mutex* mutex2;
            if (this < &src) {
                mutex1 = &mutex;
                mutex2 = &src.mutex;
            }
            else {
                mutex2 = &mutex;
                mutex1 = &src.mutex;
            }
            Guard guard1(*mutex1);
            Guard guard2(*mutex2);

            for (const auto& pair : src.map)
                add(pair.first);
        }
    }

    void erase(const SockAddr srvrAddr) {
        return remove(srvrAddr);
    }

    void erase(const Impl& src) {
        Guard srcGuard(src.mutex);
        for (const auto& pair : src.map)
            remove(pair.first);
    }

    SockAddr removeHead() {
        Lock lock{mutex};
        cond.wait(lock, [&]{return !map.empty();});
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

    bool write(Xprt xprt) const {
        Guard guard{mutex};
        auto size = (Size)map.size();
        //LOG_DEBUG("Writing size=%s", std::to_string(size).data());
        if (!xprt.write((Size)map.size()))
            return false;
        for (SockAddr srvrAddr = head; srvrAddr; srvrAddr = map.at(srvrAddr).next) {
            //LOG_DEBUG("Writing socket address %s", srvrAddr.to_string().data());
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
        //LOG_DEBUG("Read size=%s", std::to_string(size).data());
        map.clear();
        for (Size i = 0; i < size; ++i) {
            SockAddr srvrAddr;
            if (!srvrAddr.read(xprt))
                return false;
            //LOG_DEBUG("Read socket address %s", srvrAddr.to_string().data());
            add(srvrAddr);
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

bool Tracker::insert(const SockAddr& peerSrvrAddr) const {
    return pImpl->insert(peerSrvrAddr);
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

SockAddr Tracker::removeHead() const {
    return pImpl->removeHead();
}

bool Tracker::write(Xprt xprt) const {
    return pImpl->write(xprt);
}

bool Tracker::read(Xprt xprt) {
    return pImpl->read(xprt);
}

} // namespace
