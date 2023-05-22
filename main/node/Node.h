/**
 * A node in the Hycast network
 *
 *        File: Node.h
 *  Created on: Jun 3, 2020
 *      Author: Steven R. Emmerson
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

#ifndef MAIN_NODE_NODE_H_
#define MAIN_NODE_NODE_H_

#include "HycastProto.h"
#include "mcast.h"
#include "P2pMgr.h"
#include "Repository.h"
#include "SubInfo.h"

#include <memory>

//class PubRepo;
//class SubRepo;

namespace hycast {

class PeerConnSrvr;
using PeerConnSrvrPimpl = std::shared_ptr<PeerConnSrvr>;

/**
 * Interface for a Hycast node. Implementations manage incoming P2P requests. This interface is
 * implemented by both a publishing node and a subscribing node.
 */
class Node
{
public:
    /// Smart pointer to an implementation
    using Pimpl = std::shared_ptr<Node>;

    /**
     * Destroys.
     */
    virtual ~Node() noexcept {}

    /**
     * Executes this instance. Starts internal threads that execute the multicast and peer-to-peer
     * components. Doesn't return until `halt()` is called or an internal thread throws an
     * unrecoverable exception. Rethrows the first unrecoverable exception thrown by an internal
     * thread if one exists. Must be paired with `halt()`.
     *
     * @throw LogicError    Instance can't be re-executed
     * @throw SystemError   System failure
     * @throw RuntimeError  P2p server failure
     * @throw RuntimeError  Multicast failure
     * @see `halt()`
     */
    virtual void run() =0;

    /**
     * Halts execution. Does nothing if this instance isn't executing. Causes `run()` to return.
     * Doesn't block. Must be paired with `run()`. Idempotent.
     *
     * @see `run()`
     * @asyncsignalsafe  No
     */
    virtual void halt() =0;

    /**
     * Returns information on the P2P-server.
     * @return Information on the P2P-server
     */
    virtual P2pSrvrInfo getP2pSrvrInfo() const =0;

    /**
     * Returns the socket address of the peer-to-peer server. May be called immediately after
     * construction.
     *
     * @return  Socket address of peer-to-peer server
     */
    virtual SockAddr getP2pSrvrAddr() const =0;

    /**
     * Waits for a subscribing peer to connect. Blocks until that happens.
     */
    virtual void waitForPeer() = 0;

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
     * Receives a request for information on a product.
     *
     * @param[in] request      Which product
     * @return                 Requested product information. Will test false if it doesn't exist.
     */
    virtual ProdInfo recvRequest(const ProdId request) =0;

    /**
     * Receives a request for a data-segment.
     *
     * @param[in] request      Which data-segment
     * @return                 Requested data-segment. Will test false if it doesn't exist.
     */
    virtual DataSeg recvRequest(const DataSegId request) =0;
};

/**
 * Interface for a Hycast publishing node. In addition to managing incoming P2P requests,
 * implementations also multicast data-products and notify subscribing nodes.
 */
class PubNode : public Node
{
public:
    /// Smart pointer to the implementation
    using Pimpl = std::shared_ptr<PubNode>;

    /// Runtime parameters for a publishing node
    struct RunPar {
        SegSize           maxSegSize; ///< Maximum size of a data-segment
        McastPub::RunPar  mcast;      ///< Multicast component
        PubP2pMgr::RunPar p2p;        ///< Peer-to-peer component
        PubRepo::RunPar   repo;       ///< Data-product repository
         /**
          * Constructs.
          * @param[in] maxSegSize  Maximum number of bytes in a data-segment
          * @param[in] mcast       Multicast component
          * @param[in] p2p         P2P component
          * @param[in] repo        Repository component
          */
        RunPar( const SegSize            maxSegSize,
                const McastPub::RunPar&  mcast,
                const PubP2pMgr::RunPar& p2p,
                const PubRepo::RunPar&   repo)
            : maxSegSize(maxSegSize)
            , mcast(mcast)
            , p2p(p2p)
            , repo(repo)
        {}
    };

    /**
     * Returns a new instance. The instance is immediately ready to accept connections from remote
     * peers and query the repository for products to send.
     *
     * @param[in] p2pAddr         Socket address for local P2P server. It shall specify a specific
     *                            interface and not the wildcard. The port number may be 0, in which
     *                            case the operating system will choose the port.
     * @param[in] maxPeers        Maximum number of P2P peers. It shall not be 0.
     * @param[in] evalTime        Evaluation interval for poorest-performing peer in seconds
     * @param[in] mcastAddr       Socket address of multicast group
     * @param[in] mcastIfaceAddr  IP address of interface to use. If wildcard, then O/S chooses.
     * @param[in] maxPendConn     Maximum number of pending connections. 0 obtains the system default.
     * @param[in] repoRoot        Pathname of the root directory of the repository
     * @param[in] maxSegSize      Maximum size of a data-segment in bytes
     * @param[in] maxOpenFiles    Maximum number of open, data-products files
     * @throw InvalidArgument     `listenSize` is zero
     * @return                    New instance
     */
    static Pimpl create(
            const SockAddr p2pAddr,
            const unsigned maxPeers,
            const unsigned evalTime,
            const SockAddr mcastAddr,
            const InetAddr mcastIfaceAddr,
            const unsigned maxPendConn,
            const String&  repoRoot,
            const SegSize  maxSegSize,
            const long     maxOpenFiles);

    /**
     * Returns a new instance.
     * @param[in] maxSegSize    Maximum size of a data-segment in bytes
     * @param mcastRunPar       Runtime parameters for the multicast component
     * @param p2pRunPar         Runtime parameters for the P2P component
     * @param repoRunPar        Runtime parameters for the repository component
     * @return                  A new instance
     */
    static Pimpl create(
            const SegSize            maxSegSize,
            const McastPub::RunPar&  mcastRunPar,
            const PubP2pMgr::RunPar& p2pRunPar,
            const PubRepo::RunPar&   repoRunPar);

    /**
     * Destroys.
     */
    virtual ~PubNode() noexcept {}
};

/**
 * Interface for a subscribing Hycast node. Implementations manage incoming multicast transmissions
 * and incoming and outgoing P2P transmissions.
 */
class SubNode : public Node
{
public:
    /// Smart pointer to the implementation
    using Pimpl = std::shared_ptr<SubNode>;

#if 0
    /**
     * Constructs.
     *
     * @param[in] subInfo       Subscription information
     * @param[in] mcastIface    IP address of interface to receive multicast on
     * @param[in] srvrSock      Server socket for local P2P server. IP address shall not be the
     *                          wildcard.
     * @param[in] acceptQSize   Size of `RpcSrvr::accept()` queue. Don't use 0.
     * @param[in] timeout       Timeout, in ms, for connecting to remote P2P server
     * @param[in] maxPeers      Maximum number of peers. Must not be zero. Might be adjusted.
     * @param[in] evalTime      Evaluation interval for poorest-performing peer in seconds
     * @param[in] repoDir       Pathname of root directory of data-product repository
     * @param[in] maxOpenFiles  Maximum number of open files in repository
     */
    static Pimpl create(
            SubInfo&          subInfo,
            const InetAddr    mcastIface,
            const TcpSrvrSock srvrSock,
            const int         acceptQSize,
            const int         timeout,
            const unsigned    maxPeers,
            const unsigned    evalTime,
            const String&     repoDir,
            const long        maxOpenFiles);
#endif

    /**
     * Returns a new instance.
     *
     * @param[in] subInfo       Subscription information
     * @param[in] mcastIface    IP address of interface to receive multicast on
     * @param[in] peerConnSrvr  Peer-connection server
     * @param[in] timeout       Timeout, in ms, for connecting to remote P2P server
     * @param[in] maxPeers      Maximum number of peers. Must not be zero. Might be adjusted.
     * @param[in] evalTime      Evaluation interval for poorest-performing peer in seconds
     * @param[in] repoDir       Pathname of root directory of data-product repository
     * @param[in] maxOpenFiles  Maximum number of open files in repository
     * @throw     LogicError    IP address families of multicast group address and multicast
     *                          interface don't match
     */
    static Pimpl create(
            SubInfo&                subInfo,
            const InetAddr          mcastIface,
            const PeerConnSrvrPimpl peerConnSrvr,
            const int               timeout,
            const unsigned          maxPeers,
            const unsigned          evalTime,
            const String&           repoDir,
            const long              maxOpenFiles);

    /**
     * Returns a new instance.
     *
     * @param[in] subInfo       Subscription information
     * @param[in] mcastIface    IP address of interface to receive multicast on
     * @param[in] p2pSrvrAddr   Socket address for local P2P server. IP address must not be the
     *                          wildcard. If the port number is zero, then then O/S will choose an
     *                          ephemeral port number.
     * @param[in] maxPendConn   Maximum number of pending peer connections. Don't use 0.
     * @param[in] timeout       Timeout, in ms, for connecting to remote P2P server
     * @param[in] maxPeers      Maximum number of peers. Must not be zero. Might be adjusted.
     * @param[in] evalTime      Evaluation interval for poorest-performing peer in seconds
     * @param[in] repoDir       Pathname of root directory of data-product repository
     * @param[in] maxOpenFiles  Maximum number of open files in repository
     * @throw     LogicError    IP address families of multicast group address and multicast
     *                          interface don't match
     */
    static Pimpl create(
            SubInfo&          subInfo,
            const InetAddr    mcastIface,
            const SockAddr    p2pSrvrAddr,
            const int         maxPendConn,
            const int         timeout,
            const unsigned    maxPeers,
            const unsigned    evalTime,
            const String&     repoDir,
            const long        maxOpenFiles);

    /**
     * Destroys.
     */
    virtual ~SubNode() noexcept {};

    /**
     * Receives a notice about the availability of information on a product.
     *
     * @param[in] index    Index of available product
     * @retval    true     Product information should be requested
     * @retval    false    Product information should not be requested
     */
    virtual bool shouldRequest(const ProdId index) =0;

    /**
     * Receives a notice about the availability of a data-segment.
     *
     * @param[in] segId    Identifier of available data-segment
     * @retval    true     Data-segment information should be requested
     * @retval    false    Data-segment information should not be requested
     */
    virtual bool shouldRequest(const DataSegId segId) =0;

    /**
     * Receives information about a data-product from the multicast.
     *
     * @param[in] prodInfo  Product information
     */
    virtual void recvMcastData(const ProdInfo prodInfo) {
    }

    /**
     * Receives a data-segment from the multicast.
     *
     * @param[in] dataSeg  Data-segment
     */
    virtual void recvMcastData(const DataSeg dataSeg) {
    }

    /**
     * Receives information about a data-product from the P2P network.
     *
     * @param[in] prodInfo  Product information
     */
    virtual void recvP2pData(const ProdInfo prodInfo) {
    }

    /**
     * Receives a data-segment from the P2P network.
     *
     * @param[in] dataSeg  Data-segment
     */
    virtual void recvP2pData(const DataSeg dataSeg) {
    }

    /**
     * Returns the next product to process locally.
     * @return The next product to process locally
     */
    virtual ProdEntry getNextProd() {
        return ProdEntry{};
    }

    /**
     * Returns a data segment.
     * @param[in] segId  Data segment identifier
     * @return           Corresponding data segment (might be invalid)
     */
    virtual DataSeg getDataSeg(const DataSegId segId) {
        return DataSeg{};
    }
};

} // namespace

#endif /* MAIN_NODE_NODE_H_ */
