/**
 * Keeps track of peer performance in a thread-safe manner.
 *
 *        File: Bookkeeper.h
 *  Created on: Oct 17, 2019
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

#ifndef MAIN_PROTO_BOOKKEEPER_H_
#define MAIN_PROTO_BOOKKEEPER_H_

#include "Peer.h"

#include <memory>
#include <unordered_set>
#include <utility>

namespace hycast {

/**
 * Interface for monitoring of peers.
 */
class Bookkeeper
{
protected:
    class Impl;

    std::shared_ptr<Impl> pImpl;

    Bookkeeper(Impl* impl);

public:
    virtual ~Bookkeeper() noexcept =default;

    virtual bool add(const Peer peer) const =0;

    virtual bool erase(const Peer peer) const =0;
};

/**
 * Bookkeeper for a set of publisher-peers.
 */
class PubBookkeeper final : public Bookkeeper
{
    class Impl;

public:
    /**
     * Constructs.
     *
     * @param[in] maxPeers        Maximum number of peers
     * @throws std::system_error  Out of memory
     * @cancellationpoint         No
     */
    PubBookkeeper(const int maxPeers = 8);

    void requested(const Peer peer) const;

    bool add(const Peer peer)       const          override;

    Peer getWorstPeer()             const;

    bool erase(const Peer peer)    const          override;
};

/**
 * Bookkeeper for a set of subscriber-peers.
 */
class SubBookkeeper final : public Bookkeeper
{
    class Impl;

public:
    /**
     * Constructs.
     *
     * @param[in] maxPeers        Maximum number of peers
     * @throws std::system_error  Out of memory
     * @cancellationpoint         No
     */
    SubBookkeeper(int maxPeers = 8);

    /**
     * Indicates if a remote peer should be notified about available information
     * on a product. Returns false iff the remote peer has indicated that it has
     * the datum.
     *
     * @param[in] peer       Peer
     * @param[in] prodIndex  Index of the product
     * @retval    `true`     Peer should be notified
     * @retval    `false`    Peer should not be notified
     */
    bool shouldNotify(Peer peer, const ProdIndex prodIndex) const;

    /**
     * Indicates if a remote peer should be notified about an available data
     * segment. Returns false iff the remote peer has indicated that it has the
     * datum.
     *
     * @param[in] peer       Peer
     * @param[in] dataSegId  ID of the data segment
     * @retval    `true`     Peer should be notified
     * @retval    `false`    Peer should not be notified
     */
    bool shouldNotify(Peer peer, const DataSegId dataSegId) const;

    /**
     * Indicates if information on a product should be requested by a peer. If
     * yes, then the concomitant request is added to the peer's list of
     * requests; if no, then the peer is added to a list of alternative peers
     * for the request.
     *
     * @param[in] peer               Peer
     * @param[in] prodIndex          Product index
     * @return    `true`             Request should be made
     * @return    `false`            Request shouldn't be made
     * @throws    std::out_of_range  Peer is unknown
     * @throws    logicError         This request has already been made or the
     *                               peer is already alternative peer for the
     *                               request
     * @threadsafety                 Safe
     * @cancellationpoint            No
     */
    bool shouldRequest(Peer            peer,
                       const ProdIndex prodindex) const;

    /**
     * Indicates if a data segment should be requested by a peer. If yes, then
     * the concomitant request is added to the peer's list of requests; if no,
     * then the peer is added to a list of alternative peers for the request.
     *
     * @param[in] peer               Peer
     * @param[in] dataSegId          Data segment identifier
     * @return    `true`             Request should be made
     * @return    `false`            Request shouldn't be made
     * @throws    std::out_of_range  Peer is unknown
     * @throws    logicError         This request has already been made or the
     *                               peer is already alternative peer for the
     *                               request
     * @threadsafety                 Safe
     * @cancellationpoint            No
     */
    bool shouldRequest(Peer            peer,
                       const DataSegId dataSegId) const;

    /**
     * Process a peer having received product information. Nothing happens if it
     * wasn't requested by the peer; otherwise, the corresponding request is
     * removed from the peer's outstanding requests and the set of alternative
     * peers for that request is cleared.
     *
     * @param[in] peer        Peer
     * @param[in] prodIndex   Product index
     * @retval    `true`      Success
     * @retval    `false`     Product information wasn't requested
     * @throws    LogicError  Peer is unknown
     * @threadsafety          Safe
     * @exceptionsafety       Strong guarantee
     * @cancellationpoint     No
     */
    bool received(Peer            peer,
                  const ProdIndex prodIndex) const;

    /**
     * Process a peer having received a data segment. Nothing happens if it
     * wasn't requested by the peer; otherwise, the corresponding request is
     * removed from the peer's outstanding requests.
     *
     * @param[in] peer        Peer
     * @param[in] dataSegId   Data segment identifier
     * @retval    `true`      Success
     * @retval    `false`     Data segment wasn't requested
     * @threadsafety          Safe
     * @exceptionsafety       Strong guarantee
     * @cancellationpoint     No
     */
    bool received(Peer            peer,
                  const DataSegId datasegId) const;

    /**
     * Deletes the set of peers that have information on the given product.
     *
     * @param[in] prodIndex  Index of the product
     */
    void erase(const ProdIndex prodIndex) const;

    /**
     * Deletes the set of peers that have the given data segment.
     *
     * @param[in] dataSegId  ID of the data segment
     */
    void erase(const DataSegId dataSegId) const;

    /**
     * Returns the worst performing local peer. By default, the worst performing
     * peer that was constructed as a client will be returned because that's
     * what subscriber's will want to know.
     *
     * @return Worst performing peer. Will test false if no such peer exists.
     */
    Peer getWorstPeer() const;

    /**
     * Adds a peer.
     *
     * @param peer                Peer to be added
     * @retval `true`             Success
     * @retval `false`            Not added because already exists
     * @throws std::system_error  Out of memory
     * @threadsafety              Safe
     * @exceptionsafety           Basic guarantee
     * @cancellationpoint         No
     */
    bool add(const Peer peer) const override;

    /**
     * Removes a peer. The peer's outstanding requests are reassigned to the
     * best alternative peer.
     *
     * @param[in] peer     Peer to be removed.
     * @retval    `true`   Success
     * @retval    `false`  Peer is unknown
     */
    bool erase(const Peer peer) const override;
};

} // namespace

#endif /* MAIN_PROTO_BOOKKEEPER_H_ */
