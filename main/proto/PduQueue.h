/**
 * This file declares queues of protocol data units (PDU).
 *
 *   @file: PduQueue.h
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

#ifndef MAIN_PROTO_PDUQUEUE_H_
#define MAIN_PROTO_PDUQUEUE_H_

#include "HycastProto.h"

#include <memory>

namespace hycast {

/**
 * Circular index for accessing queue elements.
 */
class QueueIndex
{
public:
    using Type = uint64_t;

private:
    Type              index;
    static const Type MAX_INDEX = ~(Type)0;

public:
    explicit QueueIndex(const Type index)
        : index(index)
    {}

    QueueIndex()
        : QueueIndex(0)
    {}

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
 * Indexed, circular queue of protocol data unit (PDU) identifiers.
 */
class PduIdQueue
{
public:
    class                 Impl;

private:
    std::shared_ptr<Impl> pImpl;

public:
    PduIdQueue();

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
     * Adds a PDU ID to the queue.
     *
     * @param[in] pduId          PDU ID to be added
     * @return                   PDU ID's corresponding index
     * @throw std::out_of_range  Queue is full
     */
    QueueIndex put(const PduId pduId) const;

    /**
     * Returns the PDU ID at a given index. Blocks until that entry exists.
     *
     * @param[in] index  Index of desired PDU ID
     * @return           PDU ID at the given index
     */
    PduId get(const QueueIndex index) const;

    /**
     * Deletes the entry at a given index.
     *
     * @param[in] index  Index of entry
     */
    void erase(const QueueIndex index) const;

    /**
     * Deletes all entries up to (but excluding) a given index.
     *
     * @param[in] index  Index of entry at which to stop
     */
    void eraseTo(const QueueIndex index) const;
};

/**
 * Indexed, circular queue of a single protocol data unit (PDU).
 *
 * @tparam PDU  Type of protocol data unit
 */
template<typename PDU>
class PduQueue
{
public:
    class Impl;

private:
    std::shared_ptr<PduQueue<PDU>::Impl> pImpl;

public:
    PduQueue() =default;

    /**
     * Constructs from a PDU identifier and an initial value for the oldest
     * index.
     *
     * @param[in] pduId        PDU identifier for this instance
     * @param[in] oldestIndex  Oldest index initial value
     */
    PduQueue(const PduId      pduId,
             const QueueIndex oldestIndex);

    /**
     * Returns this instance's PDU identifier.
     *
     * @return  PDU identifier for this instance
     */
    PduId getPduId() const;

    /**
     * Adds a PDU.
     *
     * @param[in]                PDU's index
     * @throw std::out_of_range  Queue is full
     * @see `eraseTo()`
     */
    void put(const QueueIndex index, const PDU& pdu) const;

    /**
     * Returns the PDU at an index.
     *
     * @param[in] index         Index of PDU to be returned
     * @throw std::logic_error  No such element
     */
    PDU get(const QueueIndex index) const;

    /**
     * Deletes all elements from the current oldest index up to (but excluding)
     * a given index. This is necessary to prevent the queue from overflowing.
     *
     * @param[in] to  Index at which to stop deleting
     * @see `put()`
     */
    void eraseTo(const QueueIndex to) const;
};

} // namespace

#endif /* MAIN_PROTO_PDUQUEUE_H_ */
