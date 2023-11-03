/**
 * A node in the Bicast network
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

#include "Disposer.h"
#include "BicastProto.h"
#include "mcast.h"
#include "P2pMgr.h"
#include "Repository.h"
#include "SubInfo.h"

#include <memory>

namespace bicast {

class Tracker;

class PeerConnSrvr;
using PeerConnSrvrPtr = std::shared_ptr<PeerConnSrvr>;

class Node;                            ///< Forward declaration
using NodePtr = std::shared_ptr<Node>; ///< Smart pointer to an implementation

/**
 * Interface for a Bicast node. Implementations manage incoming P2P requests. This interface is
 * implemented by both a publishing node and a subscribing node.
 */
class Node
{
public:
    /**
     * Destroys.
     */
    virtual ~Node() =default;

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

/**************************************************************************************************/

class PubNode;                               ///< Forward declaration
using PubNodePtr = std::shared_ptr<PubNode>; ///< Smart pointer to an implementation

/**
 * Interface for a Bicast publishing node. In addition to managing incoming P2P requests,
 * implementations also multicast data-products and notify subscribing nodes.
 */
class PubNode : virtual public Node
{
public:
    /// Runtime parameters for a publishing node
    struct RunPar {
        /// Default size of a canonical data-segment in bytes
        static constexpr int DEF_MAX_SEGSIZE = 20000;

        String             pubRoot;    ///< Root directory for this node
        SegSize            maxSegSize; ///< Maximum size of a data-segment
        McastPub::RunPar   mcast;      ///< Runtime parameters for the multicast component
        PubP2pMgr::RunPar  p2p;        ///< Runtime parameters for the peer-to-peer component
        Repository::RunPar repo;       ///< Runtime parameters for the repository component

         /**
          * Constructs.
          * @param[in] pubRoot     Root directory of this node
          * @param[in] maxSegSize  Maximum number of bytes in a data-segment
          * @param[in] mcast       Runtime parameters for the multicast component
          * @param[in] p2p         Runtime parameters for the P2P component
          * @param[in] repo        Runtime parameters for the Repository component
          */
        RunPar( const String&             pubRoot,
                const SegSize             maxSegSize,
                const McastPub::RunPar&   mcast,
                const PubP2pMgr::RunPar&  p2p,
                const Repository::RunPar& repo)
            : pubRoot("./pubRoot")
            , maxSegSize(maxSegSize)
            , mcast(mcast)
            , p2p(p2p)
            , repo(repo)
        {}
        /// Default constructs
        RunPar()
            : RunPar("./pubRoot", DEF_MAX_SEGSIZE, McastPub::RunPar(), PubP2pMgr::RunPar(),
                    Repository::RunPar())
        {}
    };

    /**
     * Returns a new instance. The instance is immediately ready to accept connections from remote
     * peers and query the repository for products to send.
     *
     * @param[in] tracker            Tracks P2P-servers
     * @param[in] p2pAddr            Socket address for local P2P server. It shall specify a
     *                               specific interface and not the wildcard. The port number may be
     *                               0, in which case the operating system will choose the port.
     * @param[in] maxPeers           Maximum number of P2P peers. It shall not be 0.
     * @param[in] evalTime           Evaluation interval for poorest-performing peer in seconds
     * @param[in] mcastAddr          Socket address of multicast group
     * @param[in] mcastIfaceAddr     IP address of interface to use. If wildcard, then O/S chooses.
     * @param[in] maxPendConn        Maximum number of pending connections. 0 obtains the system
     *                               default.
     * @param[in] pubRoot            Pathname of the root directory of the publisher
     * @param[in] maxSegSize         Maximum size of a data-segment in bytes
     * @param[in] maxOpenFiles       Maximum number of files the repository should have open
     * @param[in] feedName           Name of the data-product feed
     * @param[in] keepTime           Maximum time, in seconds, to keep data-product files
     * @param[in] heartbeatInterval  Time interval between heartbeat packets
     * @throw InvalidArgument        `listenSize` is zero
     * @return                       New instance
     */
    static PubNodePtr create(
            Tracker&           tracker,
            const SockAddr     p2pAddr,
            const unsigned     maxPeers,
            const unsigned     evalTime,
            const SockAddr     mcastAddr,
            const InetAddr     mcastIfaceAddr,
            const unsigned     maxPendConn,
            const String&      pubRoot,
            const SegSize      maxSegSize,
            const long         maxOpenFiles,
            const int          keepTime,
            const String&      feedName,
            const SysDuration  heartbeatInterval);

    /**
     * Returns a new instance.
     * @param[in] tracker       Tracks P2P-servers
     * @param[in] maxSegSize    Maximum size of a data-segment in bytes
     * @param[in] mcastRunPar   Runtime parameters for the multicast component
     * @param[in] p2pRunPar     Runtime parameters for the P2P component
     * @param[in] pubRoot       Pathname of the root directory of the publisher
     * @param[in] repoRunPar    Runtime parameters for the publisher's repository
     * @param[in] feedName      Name of the data-product feed
     * @return                  A new instance
     */
    static PubNodePtr create(
            Tracker&                   tracker,
            const SegSize              maxSegSize,
            const McastPub::RunPar&    mcastRunPar,
            const PubP2pMgr::RunPar&   p2pRunPar,
            const String&              pubRoot,
            const Repository::RunPar&  repoRunPar,
            const String&              feedName);

    /**
     * Returns a new instance.
     * @param[in] feedName      Name of the data-product feed
     * @param[in] tracker       Tracks P2P-servers
     * @param[in] runPar        Runtime parameters for this node
     * @return                  A new instance
     */
    static PubNodePtr create(
            const String&          feedName,
            Tracker&               tracker,
            const PubNode::RunPar& runPar) {
        return create(tracker, runPar.maxSegSize, runPar.mcast, runPar.p2p, runPar.pubRoot,
                runPar.repo, feedName);
    }

    /**
     * Destroys.
     */
    virtual ~PubNode() =default;

    /**
     * Adds a data-product contained in a file.
     * @param[in] filePath  The pathname of the file
     * @param[in] prodName  The name of the data-product
     */
    virtual void addProd(
            const String& filePath,
            const String& prodName) const =0;
};

/**************************************************************************************************/

class SubNode;                               ///< Forward declaration
using SubNodePtr = std::shared_ptr<SubNode>; ///< Smart pointer to an implementation

/**
 * Interface for a subscribing Bicast node. Implementations manage incoming multicast transmissions
 * and incoming and outgoing P2P transmissions.
 */
class SubNode : virtual public Node
{
public:
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
     * Interface for a SubNode's client. As of 2023-09-04, this is used for unit/integration-testing
     * and not by a subscriber.
     */
    class Client {
    public:
        /// Destroys.
        virtual ~Client() =default;

        /**
         * Notifies the client about a data-product that was just received.
         * @param[in] prodInfo  Information on the just-received data-product
         */
        virtual void received(const ProdInfo& prodInfo) =0;
    };

    /**
     * Returns a new instance.
     *
     * @param[in] subInfo            Subscription information
     * @param[in] mcastIface         IP address of interface to receive multicast on
     * @param[in] peerConnSrvr       Peer-connection server
     * @param[in] timeout            Timeout, in ms, for connecting to remote P2P server
     * @param[in] maxPeers           Maximum number of peers. Must not be zero. Might be adjusted.
     * @param[in] evalTime           Evaluation interval for poorest-performing peer in seconds
     * @param[in] subRoot            Pathname of root directory of subscriber
     * @param[in] maxOpenFiles       Maximum number of open files in repository
     * @param[in] dispoFact          Factory for creating the SubNode's Disposer
     * @param[in] client             Pointer to SubNode's client or `nullptr`
     * @param[in] heartbeatInterval  Time interval between heartbeat packets. <0 => no heartbeat
     * @throw     LogicError         IP address families of multicast group address and multicast
     *                               interface don't match
     * @see getNextProd()
     */
    static SubNodePtr create(
            SubInfo&              subInfo,
            const InetAddr        mcastIface,
            const PeerConnSrvrPtr peerConnSrvr,
            const int             timeout,
            const unsigned        maxPeers,
            const unsigned        evalTime,
            const String&         subRoot,
            const long            maxOpenFiles,
            Disposer::Factory&    dispoFact,
            Client* const         client,
            const SysDuration     heartbeatInterval);

    /**
     * Destroys.
     */
    virtual ~SubNode() =default;

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
     * Returns a data segment.
     * @param[in] segId  Data segment identifier
     * @return           Corresponding data segment (might be invalid)
     */
    virtual DataSeg getDataSeg(const DataSegId segId) {
        return DataSeg{};
    }

    /**
     * Returns the counts of the types of protocol data units.
     * @param[out] numMcastOrig  Number of original multicast PDUs
     * @param[out] numP2pOrig    Number of original P2P PDUs
     * @param[out] numMcastDup   Number of duplicate multicast PDUs
     * @param[out] numP2pDup     Number of duplicate P2P PDUs
     */
    virtual void getPduCounts(
            long& numMcastOrig,
            long& numP2pOrig,
            long& numMcastDup,
            long& numP2pDup) const noexcept =0;

    /**
     * Returns the total number of products.
     * @return The total number of products
     */
    virtual long getTotalProds() const noexcept =0;

    /**
     * Returns the sum of the size of all products in bytes.
     * @return The sum of the size of all products in bytes
     */
    virtual long long getTotalBytes() const noexcept =0;

    /**
     * Returns the sum of the latencies of all products in seconds.
     * @return The sum of the latencies of all products in seconds
     */
    virtual double getTotalLatency() const noexcept =0;
};

} // namespace

#endif /* MAIN_NODE_NODE_H_ */
