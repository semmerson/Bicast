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

#include <memory>

//class PubRepo;
//class SubRepo;

namespace hycast {

/**
 * Interface for a Hycast node. Implementations manage incoming P2P requests. This interface is
 * implemented by both a publishing node and a subscribing node.
 */
class Node
{
public:
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
     * Doesn't block. Must be paired with `run()`.
     *
     * @see `run()`
     */
    virtual void halt() =0;

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
    using Pimpl = std::shared_ptr<PubNode>;

    struct RunPar {
        SegSize           maxSegSize; ///< Maximum size of a data-segment
        McastPub::RunPar  mcast;      ///< Multicast component
        PubP2pMgr::RunPar p2p;        ///< Peer-to-peer component
        PubRepo::RunPar   repo;       ///< Data-product repository
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
     * Creates and returns an instance. The instance is immediately ready to accept connections from
     * remote peers and query the repository for products to send.
     *
     * @param[in] p2pAddr       Socket address ffor local P2P server. It shall specify a specific
     *                          interface and not the wildcard. The port number may be 0, in which
     *                          case the operating system will choose the port.
     * @param[in] maxPeers      Maximum number of P2P peers. It shall not be 0.
     * @param[in] mcastAddr     Socket address of multicast group
     * @param[in] ifaceAddr     IP address of interface to use. If wildcard, then O/S chooses.
     * @param[in] listenSize    Size of `::listen()` queue. 0 obtains the system default.
     * @param[in] repoRoot      Pathname of the root directory of the repository
     * @param[in] maxSegSize    Maximum size of a data-segment in bytes
     * @param[in] maxOpenFiles  Maximum number of open, data-products files
     * @throw InvalidArgument   `listenSize` is zero
     * @return                  New instance
     */
    static Pimpl create(
            const SockAddr p2pAddr,
            const unsigned maxPeers,
            const SockAddr mcastAddr,
            const InetAddr ifaceAddr,
            const unsigned listenSize,
            const String&  repoRoot,
            const SegSize  maxSegSize,
            const long     maxOpenFiles);

    static Pimpl create(
            const SegSize            maxSegSize,
            const McastPub::RunPar&  mcastRunPar,
            const PubP2pMgr::RunPar& p2pRunPar,
            const PubRepo::RunPar&   repoRunPar);

    /**
     * Destroys.
     */
    virtual ~PubNode() noexcept {}

    /**
     * Links to a file (which could be a directory) that's outside the
     * repository. All regular files will be published.
     *
     * @param[in] pathname       Absolute pathname (with no trailing '/') of the
     *                           file or directory to be linked to
     * @param[in] prodName       Product name if the pathname references a file
     *                           and Product name prefix if the pathname
     *                           references a directory
     * @throws InvalidArgument  `pathname` is empty or a relative pathname
     * @throws InvalidArgument  `prodName` is invalid
     */
    virtual void link(
            const std::string& pathname,
            const std::string& prodName) {
    }
};

/**
 * Interface for a subscribing Hycast node. Implementations manage incoming multicast transmissions
 * and incoming and outgoing P2P transmissions.
 */
class SubNode : public Node
{
public:
    using Pimpl = std::shared_ptr<SubNode>;

    /**
     * Creates and returns an instance. The instance immediately becomes ready to accept connections
     * from remote peers.
     *
     * @param[in]     mcastAddr     Socket address of source-specific multicast group
     * @param[in]     srcAddr       IP address of source
     * @param[in]     ifaceAddr     IP address of interface to receive multicast on
     * @param[in]     p2pAddr       Socket address for local P2P server. It shall not specify the
     *                              wildcard. If the port number is zero, then the operating system
     *                              will choose an ephemeral port number.
     * @param[in]     maxPeers      Maximum number of peers. Must not be zero. Might be adjusted.
     * @param[in,out] tracker       Pool of remote P2P-servers
     * @param[in]     repoRoot      Pathname of the root directory of the repository
     * @param[in]     maxSegSize    Maximum size of a data-segment in bytes
     * @param[in]     maxOpenFiles  Maximum number of open, data-products files
     * @return                      An instance
     */
    static Pimpl create(
            const SockAddr mcastAddr,
            const InetAddr srcAddr,
            const InetAddr ifaceAddr,
            const SockAddr p2pAddr,
            unsigned       maxPeers,
            Tracker        tracker,
            const String&  repoRoot,
            const SegSize  maxSegSize,
            const long     maxOpenFiles);

    /**
     * Destroys.
     */
    virtual ~SubNode() noexcept {};

    /**
     * Receives a notice about the availability of information on a product.
     *
     * @param[in] index    Index of available product
     * @retval    `true`   Product information should be requested
     * @retval    `false`  Product information should not be requested
     */
    virtual bool shouldRequest(const ProdId index) =0;

    /**
     * Receives a notice about the availability of a data-segment.
     *
     * @param[in] segId    Identifier of available data-segment
     * @retval    `true`   Data-segment information should be requested
     * @retval    `false`  Data-segment information should not be requested
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

    virtual ProdInfo getNextProd() {
    }

    virtual DataSeg getDataSeg(const DataSegId segId) {
    }
};

} // namespace

#endif /* MAIN_NODE_NODE_H_ */
