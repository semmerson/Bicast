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

class Bookkeeper;                                  ///< Forward declaration
using BookkeeperPtr = std::shared_ptr<Bookkeeper>; ///< Smart pointer to an implementation

/**
 * Interface for monitoring of peers.
 */
class Bookkeeper
{
public:
    /**
     * Constructs.
     * @param[in] maxPeers  Maximum number of P2P neighbors
     * @return              Pointer to a new instance for a publisher
     */
    static BookkeeperPtr createPub(const int maxPeers = 8);

    /**
     * Constructs.
     * @param[in] maxPeers  Maximum number of P2P neighbors
     * @return              Pointer to a new instance for a subscriber
     */
    static BookkeeperPtr createSub(const int maxPeers = 8);

    virtual ~Bookkeeper() noexcept {}

    /**
     * Adds a peer.
     * @param[in] peer     The peer to be added
     * @retval    true     Success
     * @retval    false    The peer was previously added
     */
    virtual bool add(const PeerPtr peer) =0;

    /**
     * Removes a peer.
     * @param[in] peer     The peer to be removed
     * @retval    true     The peer existed
     * @retval    false    The peer didn't exist
     */
    virtual bool erase(const PeerPtr peer) =0;

    /**
     * Handles a peer requesting something from its remote counterpart.
     * @param[in] peer  The peer that made the request
     */
    virtual void requested(const PeerPtr peer) =0;

    /**
     * Indicates if a remote peer should be notified about available information
     * on a product. Returns false iff the remote peer has indicated that it has
     * the datum.
     *
     * @param[in] peer       Peer
     * @param[in] prodId     Identifier of the product
     * @retval    true       Peer should be notified
     * @retval    false      Peer should not be notified
     */
    virtual bool shouldNotify(
            PeerPtr      peer,
            const ProdId prodId) const =0;

    /**
     * Indicates if a remote peer should be notified about an available data
     * segment. Returns false iff the remote peer has indicated that it has the
     * datum.
     *
     * @param[in] peer       Peer
     * @param[in] dataSegId  ID of the data segment
     * @retval    true       Peer should be notified
     * @retval    false      Peer should not be notified
     */
    virtual bool shouldNotify(
            PeerPtr         peer,
            const DataSegId dataSegId) const =0;

    /**
     * Indicates if information on a product should be requested by a peer. If
     * yes, then the concomitant request is added to the peer's list of
     * requests; if no, then the peer is added to a list of alternative peers
     * for the request.
     *
     * @param[in] peer               Peer
     * @param[in] prodId             Product identifier
     * @return    true               Request should be made
     * @return    false              Request shouldn't be made
     * @throws    std::out_of_range  Peer is unknown
     * @throws    logicError         This request has already been made or the
     *                               peer is already alternative peer for the
     *                               request
     * @threadsafety                 Safe
     * @cancellationpoint            No
     */
    virtual bool shouldRequest(
            PeerPtr      peer,
            const ProdId prodId) =0;

    /**
     * Indicates if a data segment should be requested by a peer. If yes, then
     * the concomitant request is added to the peer's list of requests; if no,
     * then the peer is added to a list of alternative peers for the request.
     *
     * @param[in] peer               Peer
     * @param[in] dataSegId          Data segment identifier
     * @return    true               Request should be made
     * @return    false              Request shouldn't be made
     * @throws    std::out_of_range  Peer is unknown
     * @throws    logicError         This request has already been made or the
     *                               peer is already alternative peer for the
     *                               request
     * @threadsafety                 Safe
     * @cancellationpoint            No
     */
    virtual bool shouldRequest(
            PeerPtr         peer,
            const DataSegId dataSegId) =0;

    /**
     * Process a peer having received product information. Nothing happens if it
     * wasn't requested by the peer; otherwise, the corresponding request is
     * removed from the peer's outstanding requests and the set of alternative
     * peers for that request is cleared.
     *
     * @param[in] peer        Peer
     * @param[in] prodId      Product identifier
     * @retval    true        Success
     * @retval    false       Product information wasn't requested
     * @throws    LogicError  Peer is unknown
     * @threadsafety          Safe
     * @exceptionsafety       Strong guarantee
     * @cancellationpoint     No
     */
    virtual bool received(
            PeerPtr      peer,
            const ProdId prodId) =0;

    /**
     * Process a peer having received a data segment. Nothing happens if it
     * wasn't requested by the peer; otherwise, the corresponding request is
     * removed from the peer's outstanding requests.
     *
     * @param[in] peer        Peer
     * @param[in] segId       Data segment identifier
     * @retval    true        Success
     * @retval    false       Data segment wasn't requested
     * @threadsafety          Safe
     * @exceptionsafety       Strong guarantee
     * @cancellationpoint     No
     */
    virtual bool received(
            PeerPtr         peer,
            const DataSegId segId) =0;

    /**
     * Deletes the set of peers that have information on the given product.
     *
     * @param[in] prodId  Index of the product
     */
    virtual void erase(const ProdId prodId) =0;

    /**
     * Deletes the set of peers that have the given data segment.
     *
     * @param[in] segId  ID of the data segment
     */
    virtual void erase(const DataSegId segId) =0;

    /**
     * Returns the worst performing local peer. For a publisher, this will be the peer whose remote
     * counterpart made the most requests. For a subscriber, this will be the peer whose remote
     * counterpart supplied the least amount of data.
     *
     * @return Worst performing peer. Will test false if no such peer exists.
     */
    virtual PeerPtr getWorstPeer() const =0;

    /**
     * Returns the best alternative peer, besides a given one, from which to request product
     * information.
     * @param[in] peer    The peer that didn't receive the information
     * @param[in] prodId  The product's ID
     * @return            The best alternative peer
     */
    virtual PeerPtr getAltPeer(
            const PeerPtr peer,
            const ProdId  prodId) =0;

    /**
     * Returns the best alternative peer, besides a given one, from which to request a data segment.
     * @param[in] peer    The peer that didn't receive the segment
     * @param[in] segId   The data-segment's ID
     * @return            The best alternative peer
     */
    virtual PeerPtr getAltPeer(
            const PeerPtr   peer,
            const DataSegId segId) =0;

    /**
     * Resets all metrics.
     */
    virtual void reset() =0;
};

} // namespace

#endif /* MAIN_PROTO_BOOKKEEPER_H_ */
