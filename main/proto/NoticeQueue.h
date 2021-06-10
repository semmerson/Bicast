/**
 * This file declares a queue notices.
 *
 *   @file: NoticeQueue.h
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

#ifndef MAIN_PROTO_NOTICEQUEUE_H_
#define MAIN_PROTO_NOTICEQUEUE_H_

#include "HycastProto.h"
#include "Peer.h"

#include <atomic>

namespace hycast {

/**
 * Circular index for accessing the notice queue.
 */
class QueueIndex
{
public:
    using Type = uint64_t;

private:
    std::atomic<Type>              index;
    static const Type MAX_INDEX = ~(Type)0;

public:
    explicit QueueIndex(const Type index)
        : index(index)
    {}

    QueueIndex()
        : QueueIndex(0)
    {}

    QueueIndex(const QueueIndex& queueIndex)
        : index((Type)queueIndex)
    {}

    ~QueueIndex() =default;

    QueueIndex operator=(const QueueIndex& queueIndex) {
        index = (Type)queueIndex.index;
    }

    operator Type() const {
        return index;
    }

    bool operator==(const QueueIndex& rhs) const {
        return index == rhs.index;
    }

    bool operator!=(const QueueIndex& rhs) const {
        return index != rhs.index;
    }

    bool operator<(const QueueIndex& rhs) const {
        /*
         * The following expression correctly handles the values being equal.
         */
        return index - rhs.index > MAX_INDEX/2;
    }

    QueueIndex& operator++() { // Prefix version
        ++index;
        return *this;
    }

    QueueIndex operator++(int) { // Postfix version
        QueueIndex result(index);
        ++index;
        return result;
    }
};

/**
 * Indexed, circular queue of notice-type identifiers.
 */
class NoticeQueue
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    NoticeQueue(P2pMgr& p2pMgr);

    /**
     * Returns the index of the next message to be added to the queue.
     *
     * @return  Index of the next message
     */
    QueueIndex getWriteIndex() const;

    /**
     * Returns the index of the oldest message that can be read from the
     * queue.
     *
     * @return Index of oldest message
     */
    QueueIndex getOldestIndex() const;

    /**
     * Adds a path-to-publisher notice to the queue.
     *
     * @param[in] pubPath        Notice to be added
     * @return                   Notice's corresponding index
     * @throw std::out_of_range  Queue is full
     */
    QueueIndex putPubPath(const PubPath pubPath) const;

    /**
     * Adds a product-index notice to the queue.
     *
     * @param[in] prodIndex      Notice to be added
     * @return                   Notice's corresponding index
     * @throw std::out_of_range  Queue is full
     */
    QueueIndex putProdIndex(const ProdIndex prodIndex) const;

    /**
     * Adds a data-segment notice to the queue.
     *
     * @param[in] dataSegId      Notice to be added
     * @return                   Notice's corresponding index
     * @throw std::out_of_range  Queue is full
     */
    QueueIndex put(const DataSegId& dataSegId) const;

    /**
     * Delete entries up to (but excluding) a given index.
     *
     * @param[in] index  Index at which to stop
     */
    void eraseTo(const QueueIndex index) const;

    /**
     * Sends a given notice to a peer.
     *
     * @param[in] index       Index of notice to be sent
     * @param[in] peer        Peer to be sent notice
     * @throws    LogicError  No such notice
     */
    void send(const QueueIndex index, Peer peer) const;
};

} // namespace

#endif /* MAIN_PROTO_NOTICEQUEUE_H_ */
