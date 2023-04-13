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

/**
 * Interface for the peer-to-peer RPC layer.
 */
class Rpc
{
public:
    /// Smart pointer to the implementation
    using Pimpl = std::shared_ptr<Rpc>;

    /**
     * Creates a new subscriber instance within a timeout. `isClient()` will return true  .
     *
     * @param[in] srvrAddr Socket address of remote RPC server
     * @param[in] timeout  Timeout in ms. <=0 => System's default timeout.
     * @return             New instance
     * @see `isClient()`
     */
    static Pimpl create(
            const SockAddr srvrAddr,
            const int      timeout = -1);

    virtual ~Rpc() {}

    /**
     * Indicates if this instance was constructed as a client (i.e., by
     * initiating a connection to a remote RPC-server).
     *
     * @retval true     Constructed as a client
     * @retval false    Constructed by a server
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
     * Indicates if the remote peer is the publisher.
     *
     * @retval true     Yes
     * @retval false    No
     */
    virtual bool isRmtPub() const noexcept =0;

    /**
     * TODO
     * Indicates whether or not sequential delivery of PDU-s is guaranteed. TCP
     * is sequential, for example, while SCTP doesn't have to be.
     *
     * @retval true     Sequential delivery of PDU-s is guaranteed
     * @retval false    Sequential delivery of PDU-s is not guaranteed
    virtual bool isSequential() const noexcept =0;
     */

    /**
     * Starts this instance. Creates threads on which
     *   - The sockets are read; and
     *   - The peer is called.
     *
     * @param[in] peer     Peer to be called
     * @throw LogicError   Already called
     * @throw LogicError   Remote peer uses unsupported protocol
     * @throw SystemError  Thread couldn't be created
     * @see `stop()`
     */
    virtual void start(Peer& peer) =0;

    /**
     * Stops this instance from serving its remote counterpart. Stops calling
     * the `receive()` functions of the associated peer.
     *
     * Idempotent.
     *
     * @throw LogicError  Hasn't been started
     * @see   `start()`
     */
    virtual void stop() =0;

    /**
     * Runs this instance. Doesn't return until `halt()` is called or an internal thread throws an
     * exception.
     *
     * @param[in] peer        Containing peer
     * @throw LogicException  Instance was previously started
     * @see `halt()`
     */
    virtual void run(Peer& peer) =0;

    /**
     * Halts this instance. Causes `run()` to return. Doesn't block.
     *
     * @throw LogicException  Instance was not started
     * @see `run()`
     */
    virtual void halt() =0;

    /**
     * Notifies the remote as to whether this local end is a path to the publisher.
     *
     * @param[in] amPubPath  Is this end a path to the publisher?
     * @retval    true       Success
     * @retval    false      Lost connection
     */
    virtual bool notifyAmPubPath(const bool amPubPath) =0;

    /**
     * Adds a P2P-server to the remote's pool.
     *
     * @param[in] srvrAddr  Socket address of peer-server
     * @retval    true      Success
     * @retval    false     Lost connection
     */
    virtual bool add(const SockAddr srvrAddr) =0;

    /**
     * Adds P2P-servers to the remote's pool.
     *
     * @param[in] tracker   Socket addresses of peer-servers
     * @retval    true      Success
     * @retval    false     Lost connection
     */
    virtual bool add(const Tracker tracker) =0;

    /**
     * Removes a P2P-server from the remote's pool.
     *
     * @param[in] srvrAddr  Socket address of peer-server
     * @retval    true      Success
     * @retval    false     Lost connection
     */
    virtual bool remove(const SockAddr srvrAddr) =0;

    /**
     * Removes P2P-servers from the remote's pool.
     *
     * @param[in] tracker   Socket addresses of peer-servers
     * @retval    true      Success
     * @retval    false     Lost connection
     */
    virtual bool remove(const Tracker tracker) =0;

    /**
     * Notifies the remote peer about available product information. May
     * block.
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
     * Notifies the remote peer about an available data segment. May block.
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
     * Requests information on a product from the remote peer. May block.
     *
     * @param[in] prodId    Product identifier
     * @retval    true      Success
     * @retval    false     Lost connection
     */
    virtual bool request(const ProdId prodId) =0;

    /**
     * Requests a data segment from the remote peer. May block.
     *
     * @param[in] dataSegId  ID of the data segment
     * @retval    true       Success
     * @retval    false      Lost connection
     */
    virtual bool request(const DataSegId dataSegId) =0;

    /**
     * Sends information on a product to the remote peer. May block.
     *
     * @param[in] prodInfo  Product information
     * @retval    true      Success
     * @retval    false     Lost connection
     */
    virtual bool send(const ProdInfo prodInfo) =0;

    /**
     * Sends a data segment to the remote peer. May block.
     *
     * @param[in] dataSeg    The data segment
     * @retval    true       Success
     * @retval    false      Lost connection
     */
    virtual bool send(const DataSeg dataSeg) =0;

    /**
     * Sends identifiers of products to the remote peer.
     *
     * @param[in] prodIds  Product identifiers
     * @return
     */
    virtual bool send(const ProdIdSet prodIds) =0;
};

/******************************************************************************/

/**
 * Interface for an RPC-server.
 */
class RpcSrvr
{
public:
    /// Smart pointer to the implementation
    using Pimpl = std::shared_ptr<RpcSrvr>;

    /**
     * Creates a new instance.
     *
     * @param[in] srvrSock     Server socket. Local address must not be the wildcard (i.e., specify
     *                         any interface).
     * @param[in] iAmPub       Is this instance the publisher?
     * @param[in] maxPendConn  Maximum number of pending connections
     * @return                 New instance
     * @throw InvalidArgument  Server's IP address is wildcard
     * @throw InvalidArgument  Backlog argument is zero
     */
    static Pimpl create(
            const TcpSrvrSock srvrSock,
            const bool        iAmPub,
            const unsigned    maxPendConn = 8);

    /**
     * Creates a new instance.
     *
     * @param[in] srvrAddr     Address of server socket. IP address must not be the wildcard (i.e.,
     *                         specify any interface).
     * @param[in] iAmPub       Is this instance the publisher?
     * @param[in] maxPendConn  Maximum number of pending connections
     * @return                 New instance
     * @throw InvalidArgument  Server's IP address is wildcard
     * @throw InvalidArgument  Backlog argument is zero
     */
    static Pimpl create(
            const SockAddr srvrAddr,
            const bool     iAmPub,
            const unsigned maxPendConn = 8);

    virtual ~RpcSrvr() {};

    /**
     * Returns the socket address of the RPC server.
     *
     * @return Socket address of RPC server
     */
    virtual SockAddr getSrvrAddr() const =0;

    /**
     * Returns a new RPC instance that's connected to a remote RPC client. Blocks until one is
     * ready.
     *
     * @return RPC layer connected to remote RPC client. Will test false if `halt()` has been
     * called.
     */
    virtual Rpc::Pimpl accept() =0;

    /**
     * Halts the RPC-server.
     */
    virtual void halt() =0;
};

} // namespace

#endif /* MAIN_P2P_RPC_H_ */
