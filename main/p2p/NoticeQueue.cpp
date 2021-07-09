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

/**
 * Thread-unsafe queue of notice PDU ID-s.
 */
class PduIdQueue
{
private:
    using Map   = std::map<QueueIndex, PduId>;

    Map           pduIds;

public:
    PduIdQueue()
        : pduIds()
    {}

    PduIdQueue(const PduIdQueue& queue) =delete;
    PduIdQueue& operator=(const PduIdQueue& queue) =delete;

    ~PduIdQueue() noexcept =default;

    size_t size() const {
        return pduIds.size();
    }

    inline void put(const QueueIndex& index, const PduId id) {
        pduIds[index] = id;
    }

    /**
     * Indicates if the PDU ID at a given index doesn't exist.
     *
     * @param[in] index    Index of desired PDU ID
     * @retval    `true`   PDU ID does not exist
     * @retval    `false`  PDU ID does exist
     */
    inline bool empty(const QueueIndex& index) const {
        auto count = pduIds.count(index);
        return count == 0;
    }

    /**
     * Returns a reference to the PDU ID at a given index.
     *
     * @param[in] index   Index of desired PDU ID
     * @return            Reference to PDU ID
     * @throw OutOfRange  Given position is empty
     */
    inline const PduId& at(const QueueIndex& index) const {
        return pduIds.at(index);
    }

    /**
     * Deletes all entries up to (but excluding) a given index.
     *
     * @param[in] from   Index from which to start erasing
     * @param[in] to     Index of entry at which to stop
     */
    void erase(QueueIndex from, const QueueIndex& to) {
        while (from < to)
            pduIds.erase(from++);
    }
};

/******************************************************************************/

/**
 * Thread-unsafe queue of notice PDU-s.
 *
 * @tparam PDU  Notice product data unit
 */
template<typename PDU>
class PduQueue
{
    using Map   = std::map<QueueIndex, PDU>;

    Map           map;
    P2pNode&      p2pNode;
    const String  desc;

public:
    PduQueue(P2pNode& p2pNode, const String& desc)
        : map()
        , p2pNode(p2pNode)
        , desc(desc)
    {}

    /**
     * Adds an entry at a given index.
     *
     * @param[in] index   Index for entry
     * @throw LogicError  Entry already exists at index
     */
    void put(const QueueIndex& index,
             const PDU&        pdu) {
        if (map.count(index))
            throw LOGIC_ERROR("Entry already exists at index " +
                    index.to_string());
        map[index] = pdu;
    }

    void erase(QueueIndex from, const QueueIndex& to) {
        while (from < to)
            map.erase(from++);
    }

    /**
     * Sends a given notice to a peer. Blocks while sending.
     *
     * @param[in] index         Index of notice
     * @param[in] peer          Peer to be sent notice
     * @retval    `false`       Connection lost
     * @retval    `true`        Success
     * @throws    RuntimeError  Failure
     */
    bool send(const QueueIndex& index, Peer& peer) const {
        bool success;

        try {
            auto notice = map.at(index);
            success = peer.notify(notice);
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't send " + desc +
                    " notice #" + std::to_string(index) + " to peer "+
                    peer.to_string()));
        }

        return success;
    }
};

/******************************************************************************/

class NoticeQueue::Impl
{
    mutable Mutex       mutex;
    mutable Cond        cond;
    PduIdQueue          pduIdQueue;
    PduQueue<PubPath>   pubPaths;
    PduQueue<ProdIndex> prodIndexes;
    PduQueue<DataSegId> dataSegIds;
    QueueIndex          writeIndex;
    QueueIndex          oldestIndex;

    /**
     * Adds a PDU ID at the write index in the PDU ID queue. Increments the
     * write index.
     *
     * @pre                      Mutex is locked
     * @param[in] pduId          PDU ID to be added
     * @return                   PDU ID's corresponding index
     * @throw std::out_of_range  Queue is full
     * @post                     Mutex is locked
     */
    QueueIndex put(const PduId pduId) {
        LOG_ASSERT(!mutex.try_lock());

        if (writeIndex+1 == oldestIndex)
            throw OUT_OF_RANGE("Queue is full: size=" +
                    std::to_string(pduIdQueue.size()));

        const auto index = writeIndex;
        pduIdQueue.put(writeIndex++, pduId);
        cond.notify_all();

        return index;
    }

    /**
     * Returns the PDU ID at a given index. Blocks until that PDU ID exists.
     *
     * @pre              Mutex is unlocked
     * @param[in] index  Index of desired PDU ID
     * @param[in] lock   Lock of mutex
     * @return           PDU ID at the given index
     * @post             Mutex is unlocked
     */
    PduId get(const QueueIndex& index) const {
        Lock lock{mutex};

        while (pduIdQueue.empty(index))
            cond.wait(lock);

        return pduIdQueue.at(index);
    }

public:
    Impl(P2pNode& p2pNode)
        : mutex()
        , cond()
        , pduIdQueue()
        , pubPaths(p2pNode, "path-to-publisher")
        , prodIndexes(p2pNode, "product-index")
        , dataSegIds(p2pNode, "data-segment ID")
        , writeIndex(0)
        , oldestIndex(0)
    {}

    /**
     * Returns the index of the next notice to be added to the queue.
     *
     * @return  Index of the next notice
     */
    QueueIndex getWriteIndex() const {
        Guard guard(mutex);
        return writeIndex;
    }

    /**
     * Returns the index of the oldest notice in the queue.
     *
     * @return Index of oldest notice
     */
    QueueIndex getOldestIndex() const {
        Guard guard(mutex);
        return oldestIndex;
    }

    QueueIndex put(const PubPath pubPath) {
        Guard      guard{mutex};
        const auto index = put(PduId::PUB_PATH_NOTICE);
        pubPaths.put(index, pubPath);
        return index;
    }

    QueueIndex put(const ProdIndex prodIndex) {
        Guard      guard{mutex};
        const auto index = put(PduId::PROD_INFO_NOTICE);
        prodIndexes.put(index, prodIndex);
        return index;
    }

    QueueIndex put(const DataSegId& dataSegId) {
        Guard      guard{mutex};
        const auto index = put(PduId::DATA_SEG_NOTICE);
        dataSegIds.put(index, dataSegId);
        return index;
    }

    /**
     * Sends a given notice to a peer. Blocks until that notice exists and while
     * sending it.
     *
     * @param[in] index         Index of notice
     * @param[in] peer          Peer to be sent notice
     * @retval    `false`       Connection lost
     * @retval    `true`        Success
     * @throws    LogicError    Invalid PDU ID in queue
     * @throws    RuntimeError  Failure
     */
    bool send(const QueueIndex& index, Peer& peer) const {
        LOG_TRACE;

        switch (get(index)) { // Atomic
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

    // Purge queue of old notices
    void eraseTo(const QueueIndex& to) {
        Guard guard{mutex};
        pduIdQueue .erase(oldestIndex, to);
        pubPaths   .erase(oldestIndex, to);
        prodIndexes.erase(oldestIndex, to);
        dataSegIds .erase(oldestIndex, to);
        oldestIndex = to;
        --oldestIndex;
    }
};

NoticeQueue::NoticeQueue(P2pNode& p2pNode)
    : pImpl(std::make_shared<Impl>(p2pNode))
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

bool NoticeQueue::send(const QueueIndex& index, Peer& peer) const {
    pImpl->send(index, peer);
}

} // namespace
