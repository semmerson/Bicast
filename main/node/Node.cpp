/**
 * A node in the Hycast data-product distribution network.
 *
 *        File: Node.cpp
 *  Created on: Mar 9, 2020
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

#include "config.h"

#include "Node.h"

#include "Disposer.h"
#include "LastProc.h"
#include "PeerConn.h"
#include "Shield.h"
#include "ThreadException.h"

#include <pthread.h>
#include <semaphore.h>
#include <semaphore.h>
#include <thread>

namespace hycast {

class PeerConnSrvr; ///< Forward declaration
using PeerConnSrvrPtr = std::shared_ptr<PeerConnSrvr>; ///< Necessary due to co-dependency

#if 0
    FeedInfo    feedInfo;   ///< Information on data-product feed
    TcpSrvrSock srvrSock;   ///< Subscriber-server's socket
    Thread      subThread;  ///< Thread that executes subscriber-server

    /**
     * Runs the publisher's subscriber-server. Accepts connections from
     * subscribers and provides them with information on the data-feed and the
     * latest subscriber nodes.
     */
    void acceptSubs() {
        try {
            Tracker peerSrvrAddrs;

            for (;;) {
                // Keep consonant with `SubP2pNodeImpl::SubP2pNodeImpl()`
                Xprt xprt{srvrSock.accept()};
                auto protoVers = PROTOCOL_VERSION;
                const auto subId = xprt.getRmtAddr().getInetAddr().to_string();

                if (!xprt.write(PROTOCOL_VERSION) || !xprt.read(protoVers)) {
                    LOG_NOTE("Lost connection with subscriber %s",
                            subId.data());
                    if (protoVers != PROTOCOL_VERSION) {
                        LOG_NOTE("Subscriber %s uses protocol %d", subId.data(),
                                protoVers);
                    }
                    else {
                        SockAddr rmtPeerSrvrAddr;
                        if (!rmtPeerSrvrAddr.read(xprt)
                                || !feedInfo.write(xprt)
                                || !peerSrvrAddrs.write(xprt)) {
                            LOG_NOTE("Lost connection with subscriber %s",
                                    subId.data());
                        }
                        else {
                            peerSrvrAddrs.push(rmtPeerSrvrAddr);
                        }
                    }
                }
            }
        }
        catch (const std::exception& ex) {
            threadEx.set(ex);
        }
    }

    void cancelAndJoin(Thread& thread) {
        if (thread.joinable()) {
            auto status = pthread_cancel(thread.native_handle());
            if (status) {
                LOG_SYSERR("pthread_cancel() failure");
            }
            else {
                try {
                    thread.join();
                }
                catch (const std::exception& ex) {
                    LOG_ERROR(ex, "pthread_join() failure");
                }
            }
        }
    }

    // Publishing node ctor:
            subThread = Thread(&PubP2pNodeImpl::acceptSubs, this);

    // Subscribing node ctor:
        // Keep consonant with `PubP2pNodeImpl::acceptSubs()`
        Xprt       xprt{TcpClntSock(pubSrvrAddr)};
        FeedInfo   feedInfo;
        auto       protoVers = PROTOCOL_VERSION;
        const auto pubId = pubSrvrAddr.getInetAddr().to_string();

        if (!xprt.write(PROTOCOL_VERSION) || !xprt.read(protoVers))
                throw RUNTIME_ERROR("Lost connection with publisher " + pubId);

        if (protoVers != PROTOCOL_VERSION)
            throw RUNTIME_ERROR("Publisher " + pubId + " uses protocol " +
                    std::to_string(protoVers));

        if (!peerSrvrAddr.write(xprt)
                || !feedInfo.read(xprt)
                || !peerSrvrAddrs.read(xprt))
            throw RUNTIME_ERROR("Lost connection with publisher " + pubId);
#endif

/**
 * Base class for Hycast node implementations.
 */
class NodeImpl : virtual public Node
{
    /**
     * Runs the peer-to-peer component. Blocks until `P2pMgr::run()` returns.
     */
    void runP2p()
    {
        try {
            p2pMgr->run();
        }
        catch (const std::exception& ex) {
            setException();
        }
    }

protected:
    mutable Thread p2pThread;    ///< Peer-to-peer thread (either publisher's or subscriber's)
    mutable sem_t  stopSem;      ///< For async-signal-safe stopping
    BaseP2pMgrPtr  p2pMgr;       ///< Peer-to-peer component (either publisher's or subscriber's)
    ThreadEx       threadEx;     ///< Thread exception

    /**
     * Executes the P2P and multicast components on separate threads. Doesn't block.
     */
    virtual void startThreads() =0;

    /**
     * Stops all sub-threads. Doesn't block.
     */
    virtual void stopThreads() =0;

    /**
     * Sets the first exception thrown by a sub-thread.
     */
    void setException() {
        threadEx.set();
        ::sem_post(&stopSem);
    }

    /**
     * Starts the peer-to-peer component on a separate thread. Doesn't block.
     */
    void startP2p() {
        try {
            p2pThread = Thread(&NodeImpl::runP2p, this);
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't create thread to run P2P component"));
        }
    }

    /**
     * Stops the peer-to-peer component. Shouldn't block for long.
     */
    void stopP2p() {
        if (p2pThread.joinable()) {
            p2pMgr->halt();
            p2pThread.join();
        }
    }

public:
    /**
     * Constructs.
     *
     * @param[in] p2pMgr             Peer-to-peer manager
     * @throws    std::system_error  Couldn't initialize semaphore
     */
    NodeImpl(BaseP2pMgrPtr p2pMgr)
        : p2pThread()
        , stopSem()
        , p2pMgr(p2pMgr)
        , threadEx()
    {
        if (::sem_init(&stopSem, 0, 0) == -1)
            throw SYSTEM_ERROR("Couldn't initialize semaphore");
    }

    virtual ~NodeImpl() noexcept {
        //LOG_DEBUG("NodeImpl being destroyed");
        ::sem_destroy(&stopSem);
    }

    P2pSrvrInfo getP2pSrvrInfo() const override {
        return p2pMgr->getSrvrInfo();
    }

    SockAddr getP2pSrvrAddr() const override {
        return p2pMgr->getSrvrInfo().srvrAddr;
    }

    void run() override {
        startThreads();
        ::sem_wait(&stopSem);
        //LOG_DEBUG("Semaphore returned");
        //LOG_DEBUG("Stopping threads");
        stopThreads();
        threadEx.throwIfSet();
    }

    void halt() override {
        int semval = 0;
        ::sem_getvalue(&stopSem, &semval);
        if (semval < 1) {
            //LOG_DEBUG("Posting to semaphore");
            ::sem_post(&stopSem);
        }
    }

    void waitForPeer() override {
        p2pMgr->waitForSrvrPeer();
    }
};

/******************************************************************************/

/**
 * Implementation of a publishing node.
 */
class PubNodeImpl final : public PubNode, public NodeImpl
{
    McastPub::Pimpl mcastPub;     ///< Publisher's multicast component
    LastProcPtr     lastProc;     ///< Saves information on last, successfully-processed
                                  ///< data-product
    PubRepo         repo;         ///< Publisher's data-product repository
    SegSize         maxSegSize;   ///< Maximum size of a data-segment in bytes
    Thread          senderThread; ///< Thread on which products are sent via multicast and P2P

    /**
     * Sends information on a product.
     *
     * @param[in] prodInfo      Product-information to be sent
     * @throws    RuntimeError  Couldn't send
     */
    void send(const ProdInfo prodInfo)
    {
        try {
            mcastPub->multicast(prodInfo);
            p2pMgr->notify(prodInfo.getId());
        }
        catch (const std::exception& ex) {
            LOG_DEBUG("Exception thrown: %s", ex.what());
            std::throw_with_nested(RUNTIME_ERROR("Couldn't send "
                    "product-information " + prodInfo.to_string()));
        }
    }

    /**
     * Sends a data-segment
     *
     * @param[in] dataSeg       Data-segment to be sent
     * @throws    RuntimeError  Couldn't send
     */
    void send(const DataSeg& dataSeg)
    {
        try {
            mcastPub->multicast(dataSeg);
            p2pMgr->notify(dataSeg.getId());
        }
        catch (const std::exception& ex) {
            LOG_DEBUG("Exception thrown: %s", ex.what());
            std::throw_with_nested(RUNTIME_ERROR("Couldn't send data-segment " +
                    dataSeg.to_string()));
        }
    }

    /**
     * Sends new data-products in the repository via the multicast and P2P components. Meant to be
     * the start routine of a separate thread.
     */
    void runSender() {
        // TODO: Make the priority of this thread less than the handling of missed-data requests
        LOG_DEBUG("Sender thread started");
        try {
            for (auto prodEntry = repo.getNextProd(); prodEntry;
                      prodEntry = repo.getNextProd()) {

                auto& prodInfo = prodEntry.prodInfo;

                LOG_INFO("Sending product " + prodInfo.to_string());
#if 1
                // Send product-information
                send(prodInfo);

                /*
                 * Data-segment multicasting is interleaved with peer notification in hopes of
                 * reducing the chance of losing a multicast data-segment by reducing the multicast
                 * rate.
                 */
                auto prodSize = prodInfo.getSize();
                for (ProdSize offset = 0; offset < prodSize; offset += maxSegSize)
                    send(prodEntry.getDataSeg(offset));
#else
                /*
                 * This code multicasts the entire product and only then notifies the peers. This
                 * was done to verify (again) that a subscribing node won't request anything already
                 * received via multicast.
                 */
                mcastPub->multicast(prodInfo);

                auto prodId = prodInfo.getId();
                auto prodSize = prodInfo.getSize();

                for (ProdSize offset = 0; offset < prodSize; offset += maxSegSize)
                    // TODO: Test for valid segment
                    mcastPub->multicast(repo.getDataSeg(DataSegId(prodId, offset)));

                p2pMgr->notify(prodInfo.getId());

                for (ProdSize offset = 0; offset < prodSize; offset += maxSegSize)
                    // TODO: Test for valid segment
                    p2pMgr->notify(DataSegId(prodId, offset));
#endif
            }
        }
        catch (const std::exception& ex) {
            setException();
        }
        catch (...) {
            LOG_DEBUG("Sending thread cancelled");
            throw;
        }
        //LOG_DEBUG("Sending thread terminating");
    }

    void haltSender() {
        //LOG_DEBUG("Halting sender thread");
        repo.halt();
    }

    /**
     * Starts the sending component on a separate thread. Doesn't block.
     */
    void startSender() {
        try {
            senderThread = Thread(&PubNodeImpl::runSender, this);
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't create thread to run multicast "
                    "component"));
        }
    }

    /**
     * Stops the sending component. Shouldn't block for long.
     */
    void stopSender() {
        if (senderThread.joinable()) {
            //LOG_DEBUG("Halting sender");
            haltSender();
            //LOG_DEBUG("Joining sender thread");
            senderThread.join();
        }
    }

protected:
    /**
     * Executes the P2P and multicast components on separate threads. Doesn't block.
     */
    void startThreads() override {
        try {
            startP2p();
            startSender();
        }
        catch (const std::exception& ex) {
            stopThreads();
            throw;
        }
    }

    /**
     * Stops all sub-threads. Doesn't block.
     */
    void stopThreads() override {
        //LOG_DEBUG("Stopping sender");
        stopSender();
        stopP2p();
    }

public:
    /**
     * Constructs.
     *
     * @param[in] tracker         Tracks P2P-servers
     * @param[in] p2pAddr         Socket address of local P2P server. It shall specify a specific
     *                            interface and not the wildcard. The port number may be 0, in which
     *                            case the operating system will choose the port.
     * @param[in] maxPeers        Maximum number of P2P peers. It shall not be 0.
     * @param[in] evalTime        Evaluation interval for poorest-performing peer in seconds
     * @param[in] mcastAddr       Socket address of multicast group
     * @param[in] mcastIfaceAddr  IP address of interface to use. If wildcard, then O/S chooses.
     * @param[in] maxPendConn     Maximum number of pending connections
     * @param[in] repoRoot        Pathname of the root directory of the repository
     * @param[in] maxSegSize      Maximum size of a data-segment in bytes
     * @param[in] maxOpenFiles    Maximum number of open, data-products files
     * @param[in] lastProcDir     Pathname of the directory containing information on the last,
     *                            successfully-processed product-file
     * @param[in] feedName        Name of the data-product feed
     * @param[in] keepTime        Number of seconds to keep data-products before deleting them
     * @throw InvalidArgument     Invalid maximum number of peers
     * @throw InvalidArgument     `maxPendConn` is zero
     */
    PubNodeImpl(
            Tracker&           tracker,
            SockAddr           p2pAddr,
            const int          maxPeers,
            const int          evalTime,
            const SockAddr     mcastAddr,
            const InetAddr     mcastIfaceAddr,
            const int          maxPendConn,
            const String&      repoRoot,
            const SegSize      maxSegSize,
            const long         maxOpenFiles,
            const String&      lastProcDir,
            const String&      feedName,
            const int          keepTime)
        : NodeImpl(PubP2pMgr::create(tracker, *this, p2pAddr, maxPeers, maxPendConn, evalTime))
        , mcastPub(McastPub::create(mcastAddr, mcastIfaceAddr))
        , lastProc(LastProc::create(lastProcDir, feedName))
        , repo(repoRoot, maxOpenFiles, lastProc->recall(), keepTime)
        , maxSegSize{maxSegSize}
        , senderThread()
    {
        DataSeg::setMaxSegSize(maxSegSize);
        LOG_NOTE("Will multicast to group " + mcastAddr.to_string() + " on interface " +
                mcastIfaceAddr.to_string());
    }

    ~PubNodeImpl() noexcept {
        //LOG_DEBUG("Publisher-node being destroyed");
        try {
            stopThreads();
            //LOG_DEBUG("Publisher-node's threads stopped");
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex, "Couldn't stop execution");
        }
    }

    ProdIdSet subtract(ProdIdSet rhs) const override {
        return repo.subtract(rhs);
    }

    ProdIdSet getProdIds() const override {
        return repo.getProdIds();
    }

    /**
     * Receives a request for product information.
     *
     * @param[in] request      Which product
     * @return                 Requested product information. Will test false if it doesn't exist.
     */
    ProdInfo recvRequest(const ProdId request) override {
        return repo.getProdInfo(request);
    }

    /**
     * Receives a request for a data-segment.
     *
     * @param[in] request      Which data-segment
     * @return                 Requested data-segment. Will test false if it doesn't exist.
     */
    DataSeg recvRequest(const DataSegId request) override {
        return repo.getDataSeg(request);
    }
};

PubNodePtr PubNode::create(
        Tracker&           tracker,
        const SockAddr     p2pAddr,
        const unsigned     maxPeers,
        const unsigned     evalTime,
        const SockAddr     mcastAddr,
        const InetAddr     ifaceAddr,
        const unsigned     maxPendConn,
        const String&      repoRoot,
        const SegSize      maxSegSize,
        const long         maxOpenFiles,
        const String&      lastProcDir,
        const String&      feedName,
        const int          keepTime) {
    return PubNodePtr{new PubNodeImpl(tracker, p2pAddr, maxPeers, evalTime, mcastAddr, ifaceAddr,
            maxPendConn, repoRoot, maxSegSize, maxOpenFiles, lastProcDir, feedName, keepTime)};
}

PubNodePtr PubNode::create(
        Tracker&                 tracker,
        const SegSize            maxSegSize,
        const McastPub::RunPar&  mcastRunPar,
        const PubP2pMgr::RunPar& p2pRunPar,
        const PubRepo::RunPar&   repoRunPar,
        const String&            feedName) {
    return create(tracker, p2pRunPar.srvr.addr, p2pRunPar.maxPeers, p2pRunPar.evalTime, mcastRunPar.dstAddr,
            mcastRunPar.srcAddr, p2pRunPar.srvr.acceptQSize, repoRunPar.rootDir, maxSegSize,
            repoRunPar.maxOpenFiles, repoRunPar.lastProcDir, feedName, repoRunPar.keepTime);
}

/**************************************************************************************************/

/**
 * Implementation of a subscribing node.
 */
class SubNodeImpl final : public SubNode, public NodeImpl
{
    mutable Thread             disposeThread;///< Local processing of data-products thread
    mutable Thread             mcastThread;  ///< Multicast receiving thread
    Disposer                   disposer;     ///< Locally processes received data-products
    SubRepo                    repo;         ///< Data-product repository
    std::atomic<unsigned long> numMcastOrig; ///< Number of original multicast PDUs
    std::atomic<unsigned long> numP2pOrig;   ///< Number of original P2P PDUs
    std::atomic<unsigned long> numMcastDup;  ///< Number of duplicate multicast PDUs
    std::atomic<unsigned long> numP2pDup;    ///< Number of duplicate P2P PDUs
    McastSub::Pimpl            mcastSub;     ///< Subscriber's multicast component
    Client* const              client;       ///< A SubNode's client

    /**
     * Performs local processing of complete data-products. Intended as start function for a thread.
     * @see stopDisposer()
     */
    void runDisposer() {
        try {
            for (auto prodEntry = repo.getNextProd(); prodEntry;
                      prodEntry = repo.getNextProd()) {
                Shield shield{}; // Protects product disposition from cancellation

                LOG_DEBUG("Disposing of " + prodEntry.to_string());
                disposer.dispose(prodEntry.getProdInfo(), prodEntry.getData(),
                        prodEntry.getPathname());
                if (client)
                    client->received(prodEntry.getProdInfo());
            }
        }
        catch (const std::exception& ex) {
            setException();
        }
    }

    inline void startDisposer() {
        try {
            disposeThread = std::thread(&SubNodeImpl::runDisposer, this);
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't create thread to process products"));
        }
    }

    /**
     * Stops local processing of data-products. Shouldn't block for long.
     * @see runDisposer()
     */
    inline void stopDisposer() {
        if (disposeThread.joinable()) {
            repo.halt(); // Causes repo.getNextProd() to return an invalid object
            disposeThread.join();
        }
    }

    void runMcast() {
        try {
            mcastSub->run();
        }
        catch (const std::exception& ex) {
            setException();
        }
    }

    /**
     * Starts the multicast component on a separate thread. Doesn't block.
     */
    inline void startMcast() {
        try {
            mcastThread = Thread(&SubNodeImpl::runMcast, this);
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't create thread to run multicast "
                    "component"));
        }
    }

    /**
     * Stops the multicast component. Shouldn't block for long.
     */
    inline void stopMcast() {
        if (mcastThread.joinable()) {
            mcastSub->halt();
            mcastThread.join();
        }
    }

protected:
    /**
     * Executes the P2P and multicast components on separate threads. Doesn't block.
     */
    void startThreads() override {
        try {
            if (disposer)
                startDisposer();
            startP2p();
            startMcast();
        }
        catch (const std::exception& ex) {
            stopThreads();
            throw;
        }
    }

    /**
     * Stops all sub-threads. Doesn't block.
     */
    void stopThreads() override {
        stopMcast();
        stopP2p();
        stopDisposer();
    }

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
     * @param[in] maxPeers      Maximum number of peers. Must not be zero. Might be adjusted.
     * @param[in] evalTime      Evaluation interval for poorest-performing peer in seconds
     * @param[in] repoDir       Pathname of root directory of data-product repository
     * @param[in] maxSegSize    Maximum size of a data-segment in bytes
     * @param[in] maxOpenFiles  Maximum number of open files in repository
     */
    SubNodeImpl(
            SubInfo&          subInfo,
            const InetAddr    mcastIface,
            const TcpSrvrSock srvrSock,
            const int         acceptQSize,
            const unsigned    maxPeers,
            const unsigned    evalTime,
            const String&     repoDir,
            const long        maxOpenFiles)
        : NodeImpl(SubP2pMgr::create(*this, subInfo.tracker, srvrSock, acceptQSize, maxPeers,
                evalTime))
        , repo(SubRepo(repoDir, subInfo.maxSegSize, maxOpenFiles))
        , numMcastOrig{0}
        , numP2pOrig{0}
        , numMcastDup{0}
        , numP2pDup{0}
        , mcastSub{McastSub::create(subInfo.mcast.dstAddr, subInfo.mcast.srcAddr, mcastIface, *this)}
    {}
# endif

    /**
     * Constructs.
     *
     * @param[in] subInfo       Subscription information
     * @param[in] mcastIface    IP address of interface to receive multicast on
     * @param[in] peerConnSrvr  Peer-connection server
     * @param[in] timeout;      Timeout, in ms, for connecting to remote P2P server
     * @param[in] maxPeers      Maximum number of peers. Must not be zero. Might be adjusted.
     * @param[in] evalTime      Evaluation interval for poorest-performing peer in seconds
     * @param[in] repoDir       Pathname of root directory of data-product repository
     * @param[in] maxOpenFiles  Maximum number of product-files with open file descriptors
     * @param[in] disposer      Locally processes received data-products
     * @param[in] client        Pointer to the SubNode's client
     * @throw     LogicError    IP address families of multicast group address and multicast
     *                          interface don't match
     */
    SubNodeImpl(
            SubInfo&                subInfo,
            const InetAddr          mcastIface,
            const PeerConnSrvrPtr   peerConnSrvr,
            const int               timeout,
            const unsigned          maxPeers,
            const unsigned          evalTime,
            const String&           repoDir,
            const long              maxOpenFiles,
            const Disposer&         disposer,
            Client* const           client)
        : NodeImpl(SubP2pMgr::create(subInfo.tracker, *this, peerConnSrvr, timeout, maxPeers,
                evalTime))
        , disposeThread()
        , mcastThread()
        , disposer(disposer)
        , repo(SubRepo(repoDir, maxOpenFiles, disposer.getLastProcTime(), disposer.size(),
                subInfo.keepTime))
        , numMcastOrig{0}
        , numP2pOrig{0}
        , numMcastDup{0}
        , numP2pDup{0}
        , mcastSub{McastSub::create(subInfo.mcast.dstAddr, subInfo.mcast.srcAddr, mcastIface,
                *this)}
        , client(client)
    {
        DataSeg::setMaxSegSize(subInfo.maxSegSize);
        LOG_NOTE("Will receive multicast group " + subInfo.mcast.dstAddr.to_string() + " from " +
                subInfo.mcast.srcAddr.to_string() + " on interface " + mcastIface.to_string());
    }

    ~SubNodeImpl() noexcept {
        try {
            stopThreads();
            LOG_NOTE("Metrics:");
            LOG_NOTE("  Number of multicast PDUs:");
            LOG_NOTE("      Original:  " + std::to_string(numMcastOrig));
            LOG_NOTE("      Duplicate: " + std::to_string(numMcastDup));
            LOG_NOTE("  Number of P2P PDUs:");
            LOG_NOTE("      Original:  " + std::to_string(numP2pOrig));
            LOG_NOTE("      Duplicate: " + std::to_string(numP2pDup));
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex, "~SubNodeImpl() failure");
        }
    }

    ProdIdSet subtract(ProdIdSet rhs) const override {
        return repo.subtract(rhs);
    }

    ProdIdSet getProdIds() const override {
        return repo.getProdIds();
    }

    /**
     * Indicates if information on a product should be requested from the P2P network. Called by the
     * peer-to-peer component.
     *
     * @param[in] prodId     Identifier of product
     * @retval    false      Information shouldn't be requested
     * @retval    true       Information should be requested
     */
    bool shouldRequest(const ProdId prodId) override {
        //LOG_DEBUG("Determining if information on product %s should be requested",
                //prodId.to_string().data());
        return !repo.exists(prodId);
    }

    /**
     * Indicates if a data-segment should be requested from the P2P network.
     * Called by the peer-to-peer component.
     *
     * @param[in] segId      Identifier of data-segment
     * @retval    false      Data-segment shouldn't be requested
     * @retval    true       Data-segment should be requested
     */
    bool shouldRequest(const DataSegId segId) override {
        //LOG_DEBUG("Determining if data-segment %s should be requested", segId.to_string().data());
        return !repo.exists(segId);
    }

    /**
     * Receives a request for product information.
     *
     * @param[in] request      Which product
     * @return                 Requested product information. Will test false if it doesn't exist.
     */
    ProdInfo recvRequest(const ProdId request) override {
        return repo.getProdInfo(request);
    }

    /**
     * Receives a request for a data-segment.
     *
     * @param[in] request      Which data-segment
     * @return                 Requested data-segment. Will test false if it doesn't exist.
     */
    DataSeg recvRequest(const DataSegId request) override {
        return repo.getDataSeg(request);
    }

    /**
     * Processes receipt of product information from the multicast.
     *
     * @param[in] prodInfo  Product information
     */
    void recvMcastData(const ProdInfo prodInfo) override {
        if (repo.save(prodInfo)) {
            LOG_DEBUG("Saved product-information " + prodInfo.to_string());
            ++numMcastOrig;
            p2pMgr->notify(prodInfo.getId());
        }
        else {
            LOG_DEBUG("Rejected product-information %s as duplicate", prodInfo.to_string().data());
            ++numMcastDup;
        }
    }

    /**
     * Processes receipt of product information from the P2P network.
     *
     * @param[in] prodInfo  Product information
     */
    void recvP2pData(const ProdInfo prodInfo) override {
        if (repo.save(prodInfo)) {
            LOG_DEBUG("Saved product-information " + prodInfo.to_string());
            ++numP2pOrig;
        }
        else {
            LOG_DEBUG("Rejected product-information %s as duplicate", prodInfo.to_string().data());
            ++numP2pDup;
        }
    }

    /**
     * Processes receipt of a data-segment from the multicast.
     *
     * @param[in] dataSeg   Multicast data-segment
     */
    void recvMcastData(const DataSeg dataSeg) override {
        if (repo.save(dataSeg)) {
            LOG_DEBUG("Saved data-segment %s", dataSeg.getId().to_string().data());
            ++numMcastOrig;
            p2pMgr->notify(dataSeg.getId());
        }
        else {
            LOG_DEBUG("Rejected data-segment %s as duplicate", dataSeg.getId().to_string().data());
            ++numMcastDup;
        }
    }

    /**
     * Processes receipt of a data-segment from the P2P network.
     *
     * @param[in] dataSeg   Unicast data-segment
     */
    void recvP2pData(const DataSeg dataSeg) override {
        if (repo.save(dataSeg)) {
            LOG_DEBUG("Saved data-segment " + dataSeg.getId().to_string());
            ++numP2pOrig;
        }
        else {
            LOG_DEBUG("Rejected data-segment %s as duplicate", dataSeg.getId().to_string().data());
            ++numP2pDup;
        }
    }

    /**
     * Returns a data segment.
     * @param[in] segId  Segment identifier
     * @return           Corresponding data segment
     */
    DataSeg getDataSeg(const DataSegId segId) override {
        return repo.getDataSeg(segId);
    }
};

SubNodePtr SubNode::create(
        SubInfo&              subInfo,
        const InetAddr        mcastIface,
        const PeerConnSrvrPtr peerConnSrvr,
        const int             timeout,
        const unsigned        maxPeers,
        const unsigned        evalTime,
        const String&         repoDir,
        const long            maxOpenFiles,
        Disposer&             disposer,
        Client* const         client) {
    return SubNodePtr{new SubNodeImpl(subInfo, mcastIface, peerConnSrvr, timeout, maxPeers,
            evalTime, repoDir, maxOpenFiles, disposer, client)};
}

} // namespace
