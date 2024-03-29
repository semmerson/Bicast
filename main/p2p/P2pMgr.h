/**
 * This file declares the interface for a peer-to-peer manager. A P2P manager is called by local
 * peers to handle received PDU-s and by publishing nodes to notify remote peers about new data and
 * send it if necessary.It also manages the set of local peers, periodically replacing the poorest
 * performing one and exchanges information on potential peer-servers with its remote counterpart.
 * 
 * @file:   P2pMgr.h
 * @author: Steven R. Emmerson
 *
 *    Copyright 2021 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License
 * is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied. See the License for the specific language governing permissions and limitations under
 * the License.
 */

#ifndef MAIN_P2P_P2PMGR_H_
#define MAIN_P2P_P2PMGR_H_

#include "error.h"
#include "BicastProto.h"
#include "Peer.h"

#include <cstdint>
#include <memory>
#include <string>

namespace bicast {

class PeerConnSrvr; // Forward declaration
using PeerConnSrvrPtr = std::shared_ptr<PeerConnSrvr>;

class PubNode;
class SubNode;

class BaseP2pMgr;                                  ///< Forward declaration
using BaseP2pMgrPtr = std::shared_ptr<BaseP2pMgr>; ///< Smart pointer to an implementation

/**
 * Interface for a peer-to-peer manager. A publisher's P2P manager will only implement this
 * interface.
 */
class BaseP2pMgr : virtual public Peer::BaseMgr
{
public:
    /**
     * Destroys.
     */
    virtual ~BaseP2pMgr() noexcept {};

    /**
     * Returns information on this instance's P2P-server.
     *
     * @return  Information on this instance's P2P-server
     */
    virtual P2pSrvrInfo getSrvrInfo() =0;

    /**
     * Executes this instance. Starts internal threads that create, accept, and execute peers.
     * Doesn't return until `halt()` is called or an internal thread throws an  exception. Rethrows
     * the first exception thrown by an internal thread if it exists.
     *
     * @throw LogicError    Function called more than once
     * @throw SystemError   System failure
     * @throw RuntimeError  P2p server failure
     * @see                 `halt()`
     */
    virtual void run() =0;

    /**
     * Halts execution. Causes `run()` to return. Doesn't block.
     *
     * @asyncsignalsafe  Yes
     * @see              `run()`
     */
    virtual void halt() =0;

    /**
     * Receives information on P2P-servers from a remote. Updates the internal tracker. Called by a
     * peer.
     * @param[in] tracker  Information on P2P-servers
     */
    virtual void recv(const Tracker& tracker) =0;

    /**
     * Blocks until at least one remote peer has established a connection via the local P2P-server.
     * Useful for unit-testing.
     */
    virtual void waitForSrvrPeer() =0;

    /**
     * Receives a notice about a remote P2P-server. Saves the information in the tracker. Called by
     * a peer.
     * @param[in] srvrInfo  Information on the remote P2P-server
     */
    virtual void recvNotice(const P2pSrvrInfo& srvrInfo) =0;

    /**
     * Notifies connected remote peers about the availability of product information.
     *
     * @param[in] prodId  Product identifier
     */
    virtual void notify(const ProdId prodId) =0;

    /**
     * Notifies connected remote peers about the availability of a data segment.
     *
     * @param[in] dataSegId  Data segment ID
     */
    virtual void notify(const DataSegId dataSegId) =0;

    /**
     * Returns a set of this instance's identifiers of complete products minus those of another set.
     *
     * @param[in]  rhs      Other set of product identifiers to be subtracted from the ones this
     *                      instance has
     * @return              This instance's identifiers minus those of the other set
     */
    virtual ProdIdSet subtract(ProdIdSet rhs) const =0;

    /**
     * Returns the set of identifiers of complete products.
     *
     * @return             Set of complete product identifiers
     */
    virtual ProdIdSet getProdIds() const =0;

    /**
     * Returns information on a product. This might count against the remote peer.
     *
     * @param[in] prodId       Which product
     * @param[in] rmtAddr      Socket address of remote peer
     * @return                 Product information. Will test false if it
     *                         shouldn't be sent to remote peer.
     */
    virtual ProdInfo getDatum(
            const ProdId   prodId,
            const SockAddr rmtAddr) =0;
    /**
     * Returns a data-segment. This might count against the remote peer.
     *
     * @param[in] dataSegId    Which data-segment
     * @param[in] rmtAddr      Socket address of remote peer
     * @return                 Product information. Will test false if it
     *                         shouldn't be sent to remote peer.
     */
    virtual DataSeg getDatum(
            const DataSegId dataSegId,
            const SockAddr  rmtAddr) =0;
};

/**************************************************************************************************/

class PubP2pMgr;                                 ///< Forward declaration
using PubP2pMgrPtr = std::shared_ptr<PubP2pMgr>; ///< Smart pointer to an implementation

/// Interface for a publisher's P2P manager
class PubP2pMgr : public BaseP2pMgr
{
public:
    /**
     * Creates a publishing P2P manager. Creates a P2P server listening on a socket but doesn't do
     * anything with it until `run()` is called.
     *
     * @param[in] tracker            Tracks P2P-servers
     * @param[in] pubNode            Bicast publishing node
     * @throw InvalidArgument        `listenSize` is zero
     * @return                       Publisher's P2P manager
     * @see `run()`
     */
    static PubP2pMgrPtr create(
            Tracker&          tracker,
            PubNode&          pubNode);

    /**
     * Destroys.
     */
    virtual ~PubP2pMgr() noexcept =default;
};

/**************************************************************************************************/

class SubP2pMgr;                                 ///< Forward declaration
using SubP2pMgrPtr = std::shared_ptr<SubP2pMgr>; ///< Smart pointer to an implementation

/// Interface for a subscriber's P2P manager.
class SubP2pMgr : public BaseP2pMgr, public Peer::SubMgr
{
public:
    /**
     * Creates a subscribing P2P manager. Creates a P2P server listening on a socket but doesn't do
     * anything with it until `run()` is called.
     *
     * @param[in] tracker            Pool of addresses of P2P servers
     * @param[in] subNode            Subscriber's node
     * @return                       Subscribing P2P manager
     * @see `getPeerSrvrAddr()`
     */
    static SubP2pMgrPtr create(
            Tracker           tracker,
            SubNode&          subNode);

    /**
     * Creates a subscribing P2P manager. Creates a P2P server listening on a socket but doesn't do
     * anything with it until `run()` is called.
     *
     * @param[in] tracker            Pool of addresses of P2P servers
     * @param[in] subNode            Subscriber's node
     * @param[in] peerConnSrvr       Peer-connection server
     * @return                       Subscribing P2P manager
     * @see `getPeerSrvrAddr()`
     */
    static SubP2pMgrPtr create(
            Tracker           tracker,
            SubNode&          subNode,
            PeerConnSrvrPtr   peerConnSrvr);

    /**
     * Destroys.
     */
    virtual ~SubP2pMgr() noexcept =default;

    /**
     * Blocks until at least one local peer has established a connection with a remote peer.
     * Useful for unit-testing.
     */
    virtual void waitForClntPeer() =0;

    /**
     * Receives a notice of available product information from a remote peer.
     *
     * @param[in] notice       Which product
     * @param[in] rmtAddr      Socket address of remote peer
     * @retval    false        Local peer shouldn't request from remote peer
     * @retval    true         Local peer should request from remote peer
     */
    virtual bool recvNotice(const ProdId   notice,
                            const SockAddr rmtAddr) =0;
    /**
     * Receives a notice of an available data-segment from a remote peer.
     *
     * @param[in] notice       Which data-segment
     * @param[in] rmtAddr      Socket address of remote peer
     * @retval    false        Local peer shouldn't request from remote peer
     * @retval    true         Local peer should request from remote peer
     */
    virtual bool recvNotice(const DataSegId notice,
                            const SockAddr  rmtAddr) =0;

    /**
     * Handles a request for data-product information not being satisfied by a remote peer.
     *
     * @param[in] prodId     Index of the data-product
     * @param[in] rmtAddr    Socket address of remote peer
     */
    virtual void missed(
            const ProdId prodId,
            SockAddr     rmtAddr) =0;

    /**
     * Handles a request for a data-segment not being satisfied by a remote peer.
     *
     * @param[in] dataSegId  ID of data-segment
     * @param[in] rmtAddr    Socket address of remote peer
     */
    virtual void missed(const DataSegId dataSegId,
                        SockAddr        rmtAddr) =0;
    /**
     * Receives product information from a remote peer.
     *
     * @param[in] prodInfo  Product information
     * @param[in] rmtAddr   Socket address of remote peer
     */
    virtual void recvData(const ProdInfo prodInfo,
                          SockAddr       rmtAddr) =0;
    /**
     * Receives a data segment from a remote peer.
     *
     * @param[in] dataSeg  Data segment
     * @param[in] rmtAddr  Socket address of remote peer
     */
    virtual void recvData(const DataSeg dataSeg,
                          SockAddr      rmtAddr) =0;
};

} // namespace

#endif /* MAIN_P2P_P2PMGR_H_ */
