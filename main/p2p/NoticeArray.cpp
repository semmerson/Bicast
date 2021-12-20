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
#include "NoticeArray.h"

#include <map>

namespace hycast {

/**
 * Thread-unsafe queue of notice PDU ID-s.
 */
class PduIdQueue
{
private:
    using Map   = std::map<ArrayIndex, PduId>;

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

    inline void put(const ArrayIndex& index, const PduId id) {
        pduIds[index] = id;
    }

    /**
     * Indicates if the PDU ID at a given index doesn't exist.
     *
     * @param[in] index    Index of desired PDU ID
     * @retval    `true`   PDU ID does not exist
     * @retval    `false`  PDU ID does exist
     */
    inline bool empty(const ArrayIndex& index) const {
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
    inline const PduId& at(const ArrayIndex& index) const {
        return pduIds.at(index);
    }

    /**
     * Deletes all entries up to (but excluding) a given index.
     *
     * @param[in] from   Index from which to start erasing
     * @param[in] to     Index of entry at which to stop
     */
    void erase(ArrayIndex from, const ArrayIndex& to) {
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
    using Map   = std::map<ArrayIndex, PDU>;

    Map          map;
    const String desc;
    P2pMgr&      p2pMgr; ///< Associated P2P manager

public:
    PduQueue(const String& desc, P2pMgr& p2pMgr)
        : map()
        , desc(desc)
        , p2pMgr(p2pMgr)
    {}

    /**
     * Adds an entry at a given index.
     *
     * @param[in] index   Index for entry
     * @throw LogicError  Entry already exists at index
     */
    void put(const ArrayIndex& index,
             const PDU&        pdu) {
        if (map.count(index))
            throw LOGIC_ERROR("Entry already exists at index " +
                    index.to_string());
        map[index] = pdu;
    }

    void erase(ArrayIndex from, const ArrayIndex& to) {
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
    bool send(const ArrayIndex& index, Peer& peer) const {
        bool success;
        auto notice = map.at(index);

        try {
            /*
             * The P2P manager is queried at the last possible moment to
             * maximize the remote peer's opportunity to notify us and,
             * consequently, suppress notification.
             */
            success = !p2pMgr.shouldNotify(peer, notice) ||
                    peer.notify(notice);
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't notify peer " +
                    peer.to_string() + " about " + desc + " " +
                    notice.to_string()));
        }

        return success;
    }
};

/******************************************************************************/

class NoticeArray::Impl
{
    mutable Mutex       mutex;
    mutable Cond        cond;
    PduIdQueue          pduIdQueue;
    PduQueue<ProdIndex> prodIndexes;
    PduQueue<DataSegId> dataSegIds;
    ArrayIndex          writeIndex;
    ArrayIndex          oldestIndex;

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
    ArrayIndex put(const PduId pduId) {
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
    PduId get(const ArrayIndex& index) const {
        Lock lock{mutex};

        while (pduIdQueue.empty(index))
            cond.wait(lock);

        return pduIdQueue.at(index);
    }

public:
    Impl(P2pMgr& p2pMgr)
        : mutex()
        , cond()
        , pduIdQueue()
        , prodIndexes("product-index", p2pMgr)
        , dataSegIds("data-segment ID", p2pMgr)
        , writeIndex(0)
        , oldestIndex(0)
    {}

    /**
     * Returns the index of the next notice to be added to the queue.
     *
     * @return  Index of the next notice
     */
    ArrayIndex getWriteIndex() const {
        Guard guard(mutex);
        return writeIndex;
    }

    /**
     * Returns the index of the oldest notice in the queue.
     *
     * @return Index of oldest notice
     */
    ArrayIndex getOldestIndex() const {
        Guard guard(mutex);
        return oldestIndex;
    }

    ArrayIndex put(const ProdIndex prodIndex) {
        Guard      guard{mutex};
        const auto index = put(PduId::PROD_INFO_NOTICE);
        prodIndexes.put(index, prodIndex);
        return index;
    }

    ArrayIndex put(const DataSegId& dataSegId) {
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
    bool send(const ArrayIndex& index, Peer& peer) const {
        bool success;
        auto pduId = get(index);
        if (PduId::PROD_INFO_NOTICE == pduId) {
            success = prodIndexes.send(index, peer);
        }
        else if (PduId::DATA_SEG_NOTICE == pduId) {
            success = dataSegIds.send(index, peer);
        }
        else {
            throw LOGIC_ERROR("Invalid PDU ID");
        }
        return success;
    }

    // Purge queue of old notices
    void eraseTo(const ArrayIndex& to) {
        Guard guard{mutex};
        pduIdQueue .erase(oldestIndex, to);
        prodIndexes.erase(oldestIndex, to);
        dataSegIds .erase(oldestIndex, to);
        oldestIndex = to;
        --oldestIndex;
    }
};

NoticeArray::NoticeArray(P2pMgr& p2pMgr)
    : pImpl(new Impl(p2pMgr))
{}

ArrayIndex NoticeArray::getWriteIndex() const {
    return pImpl->getWriteIndex();
}

ArrayIndex NoticeArray::getOldestIndex() const {
    return pImpl->getOldestIndex();
}

ArrayIndex NoticeArray::putProdIndex(const ProdIndex prodIndex) const {
    return pImpl->put(prodIndex);
}

ArrayIndex NoticeArray::put(const DataSegId& dataSegId) const {
    return pImpl->put(dataSegId);
}

void NoticeArray::eraseTo(const ArrayIndex index) const {
    pImpl->eraseTo(index);
}

bool NoticeArray::send(const ArrayIndex& index, Peer& peer) const {
    return pImpl->send(index, peer);
}

} // namespace
