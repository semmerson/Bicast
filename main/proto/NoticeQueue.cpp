/**
 * This file implements a queue of notices.
 *
 *   @file: NoticeQueue.cpp
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
#include "NoticeQueue.h"

#include <map>

namespace hycast {

class PduIdQueue
{
private:
    using Map   = std::map<QueueIndex, PduId>;

    mutable Mutex mutex;
    mutable Cond  cond;
    Map           pduIds;
    QueueIndex    writeIndex;
    QueueIndex    oldestIndex;

public:
    PduIdQueue()
        : mutex()
        , cond()
        , pduIds()
        , writeIndex(0)
        , oldestIndex(0)
    {}

    PduIdQueue(const PduIdQueue& queue) =delete;
    PduIdQueue& operator=(const PduIdQueue& queue) =delete;

    ~PduIdQueue() noexcept =default;

    /**
     * Returns the index of the next message to be added to the queue.
     *
     * @return  Index of the next message
     */
    QueueIndex getWriteIndex() const {
        Guard guard(mutex);
        return writeIndex;
    }

    /**
     * Returns the index of the oldest message in the queue.
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

        if (writeIndex+1 == oldestIndex)
            throw OUT_OF_RANGE("Queue is full: size=" +
                    std::to_string(pduIds.size()));

        const auto index = writeIndex;
        pduIds[writeIndex++] = pduId;
        cond.notify_all();

        return index;
    }

    /**
     * Returns the PDU ID at a given index. Blocks until that entry exists;
     * consequently, the current thread must be cancelled before this instance
     * is destroyed if this function is called.
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
     * Deletes all entries up to (but excluding) a given index.
     *
     * @param[in] index  Index of entry at which to stop
     */
    void eraseTo(const QueueIndex index) {
        Guard guard{mutex};
        while (oldestIndex < index)
            pduIds.erase(oldestIndex++);
    }
};

/******************************************************************************/

template<typename PDU>
class PduQueue
{
    using Map   = std::map<QueueIndex, PDU>;

    mutable Mutex mutex;
    Map           map;
    QueueIndex    oldestIndex;
    P2pMgr&       p2pMgr;
    const String  desc;

public:
    PduQueue(P2pMgr& p2pMgr, const String desc)
        : mutex()
        , map()
        , oldestIndex(0)
        , p2pMgr(p2pMgr)
        , desc(desc)
    {}

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

    /**
     * Sends a given notice to a peer. Blocks while sending.
     *
     * @param[in] index         Index of notice
     * @param[in] peer          Peer to be sent notice
     * @retval    `false`       Remote peer disconnected
     * @retval    `true`        Success
     * @throws    RuntimeError  Failure
     */
    bool send(const QueueIndex index, Peer peer) const {
        try {
            PDU notice{};
            {
                Guard guard(mutex);
                notice = map.at(index);
            }
            // Don't make foreign calls with locked mutex
            return peer.notify(notice);
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't send " + desc +
                    " notice #" + std::to_string(index) + " to peer "+
                    peer.to_string()));
        }
    }
};

/******************************************************************************/

class NoticeQueue::Impl
{
    mutable Mutex       mutex;
    PduIdQueue          pduIdQueue;
    PduQueue<PubPath>   pubPaths;
    PduQueue<ProdIndex> prodIndexes;
    PduQueue<DataSegId> dataSegIds;

public:
    Impl(P2pMgr& p2pMgr)
        : mutex()
        , pduIdQueue()
        , pubPaths(p2pMgr, "path-to-publisher")
        , prodIndexes(p2pMgr, "product-index")
        , dataSegIds(p2pMgr, "data-segment ID")
    {}

    QueueIndex getWriteIndex() const {
        return pduIdQueue.getWriteIndex();
    }

    QueueIndex getOldestIndex() const {
        return pduIdQueue.getOldestIndex();
    }

    QueueIndex put(const PubPath pubPath) {
        Guard      guard{mutex};
        const auto index = pduIdQueue.put(PduId::PUB_PATH_NOTICE);
        pubPaths.put(index, pubPath);
        return index;
    }

    QueueIndex put(const ProdIndex prodIndex) {
        Guard      guard{mutex};
        const auto index = pduIdQueue.put(PduId::PROD_INFO_NOTICE);
        prodIndexes.put(index, prodIndex);
        return index;
    }

    QueueIndex put(const DataSegId& dataSegId) {
        Guard      guard{mutex};
        const auto index = pduIdQueue.put(PduId::DATA_SEG_NOTICE);
        dataSegIds.put(index, dataSegId);
        return index;
    }

    /**
     * Sends a given notice to a peer.
     *
     * @param[in] index         Index of notice
     * @param[in] peer          Peer to be sent notice
     * @retval    `false`       Remote peer disconnected
     * @retval    `true`        Success
     * @throws    LogicError    Invalid PDU ID
     * @throws    RuntimeError  Failure
     */
    bool send(const QueueIndex index, Peer peer) const {
        LOG_TRACE;

        switch (pduIdQueue.get(index)) {
        case PduId::PUB_PATH_NOTICE:
            return pubPaths.send(index, peer);
        case PduId::PROD_INFO_NOTICE:
            return prodIndexes.send(index, peer);
        case PduId::DATA_SEG_NOTICE:
            return dataSegIds.send(index, peer);
        default:
            throw LOGIC_ERROR("Invalid PDU ID");
        }
    }

    // Purge queue of entries that will no longer be read
    void eraseTo(const QueueIndex index) {
        Guard guard{mutex};
        pduIdQueue .eraseTo(index);
        pubPaths   .eraseTo(index);
        prodIndexes.eraseTo(index);
        dataSegIds .eraseTo(index);
    }
};

NoticeQueue::NoticeQueue(P2pMgr& p2pMgr)
    : pImpl(std::make_shared<Impl>(p2pMgr))
{}

QueueIndex NoticeQueue::getWriteIndex() const {
    return pImpl->getWriteIndex();
}

QueueIndex NoticeQueue::getOldestIndex() const {
    return pImpl->getOldestIndex();
}

QueueIndex NoticeQueue::putPubPath(const PubPath pubPath) const {
    return pImpl->put(pubPath);
}

QueueIndex NoticeQueue::putProdIndex(const ProdIndex prodIndex) const {
    return pImpl->put(prodIndex);
}

QueueIndex NoticeQueue::put(const DataSegId& dataSegId) const {
    return pImpl->put(dataSegId);
}

void NoticeQueue::eraseTo(const QueueIndex index) const {
    pImpl->eraseTo(index);
}

bool NoticeQueue::send(const QueueIndex index, Peer peer) const {
    pImpl->send(index, peer);
}

} // namespace
