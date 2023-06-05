/**
 * This file declares interfaces for a P2P RPC module.
 *
 *  @file:  Rpc.h
 * @author: Steven R. Emmerson <emmerson@ucar.edu>
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

#ifndef MAIN_P2P_RPC_H_
#define MAIN_P2P_RPC_H_

#include "HycastProto.h"
#include "Peer.h"
#include "SockAddr.h"

#include <memory>

namespace hycast {

class Tracker;
class PubPeer;
class SubPeer;
class ProdId;
class DataSegId;
class ProdInfo;
class DataSeg;

/// Forward declaration
class Rpc;
/// Smart pointer to an implementation
using RpcPtr = std::shared_ptr<Rpc>;

/**
 * Interface for the peer-to-peer RPC layer.
 */
class Rpc
{
public:
    /**
     * Creates a default instance.
     * @return A default instance
     */
    static RpcPtr create();

    /// Destroys.
    virtual ~Rpc() {}

    /**
     * Sends information on the local P2P-server to the remote peer. Called by a peer connection.
     * @param[in] xprt      Transport on which to send the information
     * @param[in] srvrInfo  Information on the local P2P-server
     * @retval    true      Success
     * @retval    false     Lost connection
     */
    virtual bool send(
            Xprt&              xprt,
            const P2pSrvrInfo& srvrInfo) =0;

    /**
     * Sends information on P2P-servers to the remote peer. Called by a peer connection.
     * @param[in] xprt      Transport on which to send the information
     * @param[in] tracker   Information on P2P-servers
     * @retval    true      Success
     * @retval    false     Lost connection
     */
    virtual bool send(
            Xprt&          xprt,
            const Tracker& tracker) =0;

    /**
     * Receives and dispatches the next, incoming RPC message. Called by a peer connection.
     * @param[in] xprt   Transport from which the RPC message will be read
     * @param[in] peer   Associated peer
     * @retval    true   Success
     * @retval    false  Connection lost
     */
    virtual bool recv(
            Xprt& xprt,
            Peer& peer) =0;

    /**
     * Notifies the remote about the local P2P-server.
     * @param[in] xprt      Transport on which to send the information
     * @param[in] srvrInfo  Information on the local P2P-server
     * @retval    true      Success
     * @retval    false     Lost connection
     */
    virtual bool notify(
            Xprt&              xprt,
            const P2pSrvrInfo& srvrInfo) =0;

    /**
     * Notifies the remote peer about available product information. May block.
     *
     * @param[in] xprt        Transport on which the notice will be sent
     * @param[in] prodId      Product identifier
     * @retval    true        Success
     * @retval    false       Failure
     * @throw     LogicError  Remote peer is publisher
     * @throw     LogicError  Instance isn't in started state
     * @see       `start()`
     */
    virtual bool notify(
            Xprt&        xprt,
            const ProdId prodId) =0;

    /**
     * Notifies the remote peer about an available data segment. May block.
     *
     * @param[in] xprt        Transport on which the notice will be sent
     * @param[in] dataSegId   Identifier of the data segment
     * @retval    true        Success
     * @retval    false       Failure
     * @throw     LogicError  Remote peer is publisher
     * @throw     LogicError  Instance isn't in started state
     * @see       `start()`
     */
    virtual bool notify(
            Xprt&           xprt,
            const DataSegId dataSegId) =0;

    /**
     * Requests information on a product from the remote peer. May block.
     *
     * @param[in] xprt      Transport on which the request will be sent
     * @param[in] prodId    Product identifier
     * @retval    true      Success
     * @retval    false     Lost connection
     */
    virtual bool request(
            Xprt&        xprt,
            const ProdId prodId) =0;

    /**
     * Requests a data segment from the remote peer. May block.
     *
     * @param[in] xprt       Transport on which the request will be sent
     * @param[in] dataSegId  ID of the data segment
     * @retval    true       Success
     * @retval    false      Lost connection
     */
    virtual bool request(
            Xprt&           xprt,
            const DataSegId dataSegId) =0;

    /**
     * Requests available but not previously-received products.
     * @param[in] xprt     Transport on which the set will be sent
     * @param[in] prodIds  Set of identifiers of previously-received products.
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    virtual bool request(
            Xprt&            xprt,
            const ProdIdSet& prodIds) =0;

    /**
     * Sends information on a product to the remote peer. May block.
     *
     * @param[in] xprt      Transport on which the information will be sent
     * @param[in] prodInfo  Product information
     * @retval    true      Success
     * @retval    false     Lost connection
     */
    virtual bool send(
            Xprt&          xprt,
            const ProdInfo prodInfo) =0;

    /**
     * Sends a data segment to the remote peer. May block.
     *
     * @param[in] xprt       Transport on which the data-segment will be sent
     * @param[in] dataSeg    The data segment
     * @retval    true       Success
     * @retval    false      Lost connection
     */
    virtual bool send(
            Xprt&         xprt,
            const DataSeg dataSeg) =0;
};

} // namespace

#endif /* MAIN_P2P_RPC_H_ */
