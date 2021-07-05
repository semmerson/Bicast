/**
 * A thread-safe queue of things to be sent to remote peers.
 *
 *        File: ThingIdQueue.h
 *  Created on: Jun 18, 2019
 *      Author: Steven R. Emmerson
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

#ifndef MAIN_PEER_THINGIDQUEUE_H_
#define MAIN_PEER_THINGIDQUEUE_H_

#include <PeerProto.h>
#include "error.h"
#include "hycast.h"
#include <iterator>
#include <memory>

namespace hycast {

/**
 * A thread-safe queue of chunk identifiers to be sent to a remote peer.
 */
class ChunkIdQueue final
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

public:
    class Iterator : public std::iterator<std::input_iterator_tag, ChunkId>
    {
    public:
        class                 Impl;

    public:
        std::shared_ptr<Impl> pImpl;

        Iterator(Impl* impl);

    public:
        Iterator(const Iterator& that);

        Iterator& operator=(const Iterator& rhs);

        bool operator==(const Iterator& rhs);

        bool operator!=(const Iterator& rhs);

        ChunkId operator*();

        Iterator& operator++();

        Iterator operator++(int);
    };

    ChunkIdQueue();

    size_t size() const noexcept;

    void push(ChunkId chunkId) const;

    /**
     * Removes and returns the next chunk identifier.
     *
     * @return Next chunk identifier. Will test false if `close()` has been
     *         called.
     */
    ChunkId pop() const;

    void close() const noexcept;

    bool closed() const noexcept;

    /**
     * Returns an iterator to the contents of the queue in FIFO order.
     *
     * @return Iterator to contents of queue in FIFO order
     */
    Iterator begin();

    /**
     * Returns an iterator to just beyond the last element of the queue.
     *
     * @return Iterator to just beyond last element of queue
     */
    Iterator end();
};

} // namespace

#endif /* MAIN_PEER_THINGIDQUEUE_H_ */
