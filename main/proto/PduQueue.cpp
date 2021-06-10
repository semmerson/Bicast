/**
 * This file implements queues of protocol data units (PDU).
 *
 *   @file: PduQueue.cpp
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

#include "error.h"
#include "PduQueue.h"

#include <condition_variable>
#include <map>

namespace hycast {

class PduIdQueue::Impl
{
private:
    using Map   = std::map<QueueIndex, PduId>;

    Map                pduIds;
    mutable Mutex      mutex;
    mutable Cond       cond;
    QueueIndex         nextWrite;
    QueueIndex         oldestIndex;
    bool               done;

public:
    Impl()
        : pduIds()
        , mutex()
        , cond()
        , nextWrite(0)
        , oldestIndex(0)
        , done(false)
    {}

    Impl(const PduIdQueue& queue) =delete;
    Impl& operator=(const PduIdQueue& queue) =delete;

    ~Impl() =default;

    /**
     * Returns the index of the next message to be added to the queue.
     *
     * @return  Index of the next message
     */
    QueueIndex getNextWrite() const {
        Guard guard(mutex);
        return nextWrite;
    }

    /**
     * Returns the index of the oldest message that can be read from the
     * queue.
     *
     * @return Index of oldest message
     */
    QueueIndex getOldestIndex() const {
        Guard guard(mutex);
        return oldestIndex;
    }

    /**
     * Adds a PDU ID to the queue.
     *
     * @param[in] pduId          PDU ID to be added
     * @return                   PDU ID's corresponding index
     * @throw std::out_of_range  Queue is full
     */
    QueueIndex put(const PduId pduId) {
        Guard guard{mutex};

        if (nextWrite+1 == oldestIndex)
            throw OUT_OF_RANGE("Queue is full: size=" +
                    std::to_string(pduIds.size()));

        const auto index = nextWrite;
        pduIds[nextWrite++] = pduId;
        cond.notify_all();
        return index;
    }

    /**
     * Returns the PDU ID at a given index. Blocks until that entry exists.
     *
     * @param[in] index  Index of desired PDU ID
     * @return           PDU ID at the given index
     */
    PduId get(const QueueIndex index) const {
        Lock lock{mutex};

        while (pduIds.count(index) == 0)
            cond.wait(lock);

        return pduIds.at(index);
    }

    /**
     * Deletes the entry at a given index.
     *
     * @param[in] index  Index of entry
     */
    void erase(const QueueIndex index) {
        Guard guard{mutex};
        pduIds.erase(index);
    }

    /**
     * Deletes all entries up to (but excluding) a given index.
     *
     * @param[in] index  Index of entry at which to stop
     */
    void eraseTo(const QueueIndex index) {
        Guard guard{mutex};
        while (oldestIndex != index)
            pduIds.erase(oldestIndex++);
    }
};

/******************************************************************************/

PduIdQueue::PduIdQueue()
    : pImpl(std::make_shared<Impl>())
{}

QueueIndex PduIdQueue::getNextWrite() const {
    return pImpl->getNextWrite();
}

QueueIndex PduIdQueue::getOldestIndex() const {
    return pImpl->getOldestIndex();
}

QueueIndex PduIdQueue::put(const PduId pduId) const {
    return pImpl->put(pduId);
}

PduId PduIdQueue:: get(const QueueIndex index) const {
    return pImpl->get(index);
}

void PduIdQueue::erase(const QueueIndex index) const {
    pImpl->erase(index);
}

void PduIdQueue::eraseTo(const QueueIndex index) const {
    pImpl->eraseTo(index);
};

/******************************************************************************/

template<typename PDU>
class PduQueue<PDU>::Impl
{
    using Map   = std::map<QueueIndex, PDU>;

    mutable Mutex mutex;
    Map           map;
    PduId         pduId;
    QueueIndex    oldestIndex;

public:
    Impl(const PduId      pduId,
         const QueueIndex oldestIndex)
        : mutex()
        , map()
        , pduId(pduId)
        , oldestIndex(oldestIndex)
    {}

    PduId getPduId() const {
        return pduId;
    }

    /**
     * @throw std::out_of_range  Queue is full
     */
    void put(const QueueIndex index,
             const PDU&       pdu) {
        Guard guard(mutex);
        if (map.count(index))
            throw OUT_OF_RANGE("Notification queue is full!");
        map[index] = pdu;
        if (index < oldestIndex)
            oldestIndex = index;
    }

    /**
     * @throw std::logic_error  No such element
     */
    PDU get(const QueueIndex index) const {
        Guard guard(mutex);
        if (map.count(index) == 0)
            throw LOGIC_ERROR("No entry for index " + std::to_string(index));
        return map.at(index);
    }

    void eraseTo(const QueueIndex to) {
        Guard guard(mutex);
        while (oldestIndex < to)
            map.erase(oldestIndex++);
    }
};

/******************************************************************************/

template<typename PDU>
PduQueue<PDU>::PduQueue(const PduId      pduId,
                        const QueueIndex oldestIndex)
    : pImpl(std::make_shared<Impl>(pduId, oldestIndex))
{}

template<typename PDU>
PduId PduQueue<PDU>::getPduId() const {
    return pImpl->getPduId();
}

template<typename PDU>
void PduQueue<PDU>::put(const QueueIndex index, const PDU& pdu) const {
    pImpl->put(index, pdu);
}

template<typename PDU>
PDU PduQueue<PDU>::get(const QueueIndex index) const {
    return pImpl->get(index);
}

template<typename PDU>
void PduQueue<PDU>::eraseTo(const QueueIndex to) const {
    pImpl->eraseTo(to);
}

/******************************************************************************/

template class PduQueue<PubPath>;
template class PduQueue<ProdIndex>;
template class PduQueue<DataSegId>;

} // namespace
