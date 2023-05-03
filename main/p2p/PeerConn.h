/**
 * @file PeerConn.h
 * The connection between two peers.
 * Interfaces between a Peer and the Rpc layer by hiding the number of socket connections and
 * threads from both.
 *
 *  Created on: Apr 24, 2023
 *      Author: Steven R. Emmerson
 *
 *    Copyright 2023 University Corporation for Atmospheric Research
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

#ifndef MAIN_P2P_PEERCONN_H_
#define MAIN_P2P_PEERCONN_H_

//#include "Peer.h"
#include "Socket.h"

#include <memory>

namespace hycast {

class Peer;                              ///< Forward declaration
using PeerPimpl = std::shared_ptr<Peer>; ///< Smart pointer to a peer

/// Interface for the connection between a local and remote peer.
class PeerConn
{
public:
    /// Smart pointer to an implementation
    using Pimpl = std::shared_ptr<PeerConn>;

    /**
     * Returns a new, client-side instance (i.e., one that initiated the connection). Such instances
     * are always for subscribing peers.
     * @param[in] srvrAddr Socket address of remote P2P-server
     * @param[in] timeout  Timeout in ms. <=0 => System's default timeout.
     * @return             A new, client-side instance
     */
    static Pimpl create(
            const SockAddr& srvrAddr,
            const int       timeout = -1);

    /// Destroys.
    virtual ~PeerConn() {};

    /**
     * Indicates if this instance was client-side  constructed (i.e., that it initiated the
     * connection).
     * @retval true   This instance was client-side. constructed
     * @retval false  This instance was not client-side. constructed
     */
    virtual bool isClient() const noexcept =0;

    /**
     * Returns the identifying local socket address.
     *
     * @return Identifying local socket address
     */
    virtual SockAddr getLclAddr() const noexcept =0;

    /**
     * Returns the identifying remote socket address.
     *
     * @return  Identifying remote socket address
     */
    virtual SockAddr getRmtAddr() const noexcept =0;

    /**
     * Returns the string representation of this instance.
     * @return The string representation of this instance
     */
    virtual String to_string() const =0;

    /**
     * Sets the peer to be associated with this instance.
     * @param[in] peer  The associated peer
     */
    virtual void setPeer(PeerPimpl& peer) noexcept =0;

    /**
     * Runs this instance. Doesn't return until
     *   - The connection is lost;
     *   - An error occurs; or
     *   - `halt()` * is called.
     * @throw InvalidArgument  Null pointer
     * @throw LogicError       This function already called
     * @see halt()
     */
    virtual void run() =0;

    /**
     * Causes `run()` to return. Doesn't block.
     * @see run()
     * @asyncsignalsafe  Yes
     */
    virtual void halt() =0;

    /**
     * Adds information about a P2P-server to the remote's tracker.
     *
     * @param[in] srvrInfo  Information on the P2P-server
     * @retval    true      Success
     * @retval    false     Lost connection
     */
    virtual bool add(const P2pSrvrInfo& srvrInfo) =0;

    /**
     * Adds P2P-servers to the remote's tracker.
     *
     * @param[in] tracker   Socket addresses of P2P-servers
     * @retval    true      Success
     * @retval    false     Lost connection
     */
    virtual bool add(const Tracker& tracker) =0;

    /**
     * Notifies the remote peer about available product information. Might block.
     *
     * @param[in] prodId      Product identifier
     * @retval    true        Success
     * @retval    false       Failure
     * @throw     LogicError  Remote peer is publisher
     * @throw     LogicError  Instance isn't in started state
     * @see       `start()`
     */
    virtual bool notify(const ProdId prodId) =0;

    /**
     * Notifies the remote peer about an available data segment. Might block.
     *
     * @param[in] dataSegId   Identifier of the data segment
     * @retval    true        Success
     * @retval    false       Failure
     * @throw     LogicError  Remote peer is publisher
     * @throw     LogicError  Instance isn't in started state
     * @see       `start()`
     */
    virtual bool notify(const DataSegId dataSegId) =0;

    /**
     * Requests information on a product from the remote peer. Might block.
     *
     * @param[in] prodId    Product identifier
     * @retval    true      Success
     * @retval    false     Lost connection
     */
    virtual bool request(const ProdId prodId) =0;

    /**
     * Requests a data segment from the remote peer. Might block.
     *
     * @param[in] dataSegId  ID of the data segment
     * @retval    true       Success
     * @retval    false      Lost connection
     */
    virtual bool request(const DataSegId dataSegId) =0;

    /**
     * Requests the identifiers of available products.
     * @param[in] prodIds  Set of identifiers of previously-received products
     * @retval    true     Success
     * @retval    false    Lost connection
     */
    virtual bool request(const ProdIdSet& prodIds) =0;

    /**
     * Sends information on a product to the remote peer. Might block.
     *
     * @param[in] prodInfo  Product information
     * @retval    true      Success
     * @retval    false     Lost connection
     */
    virtual bool send(const ProdInfo prodInfo) =0;

    /**
     * Sends a data segment to the remote peer. Might block.
     *
     * @param[in] dataSeg    The data segment
     * @retval    true       Success
     * @retval    false      Lost connection
     */
    virtual bool send(const DataSeg dataSeg) =0;
};

/**
 * Interface for a peer-connection server. Such a server returns server-side peer-connections (i.e.,
 * connections resulting from a remote, client-side peer-connection). Such connections may be for
 * both the publisher and subscribers.
 */
class PeerConnSrvr
{
public:
    using Pimpl = std::shared_ptr<PeerConnSrvr>; ///< Smart pointer to an instance

    /**
     * Returns a new instance.
     * @param[in] srvrAddr     Socket address for the server. Must not be the wildcard (i.e.,
     *                         specify any interface). A port number of 0 obtains a system chosen
     *                         one.
     * @param[in] maxPendConn  Maximum number of pending connections
     * @return                 New instance
     * @throw InvalidArgument  Server's IP address is wildcard
     * @throw InvalidArgument  Maximum number of pending connections is zero
     */
    static Pimpl create(
            const SockAddr& srvrAddr,
            const int       maxPendConn = 8);

    /// Destroys.
    virtual ~PeerConnSrvr() {};

    /**
     * Returns the socket address of the peer-connection server.
     * @return Socket address of P2P-server
     */
    virtual SockAddr getSrvrAddr() const =0;

    /**
     * Returns a new, server-side peer-connection. Blocks until one is ready or `halt()` is called.
     * @return A new, server-side peer-connection. Will test false if `halt()` has been called
     * @see halt()
     */
    virtual PeerConn::Pimpl accept() =0;

    /**
     * Causes `accept()` to return a false object.
     * @see accept()
     */
    virtual void halt() =0;
};

} // namespace

#endif /* MAIN_P2P_PEERCONN_H_ */
