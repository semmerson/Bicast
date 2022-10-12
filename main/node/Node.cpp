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

#include "ThreadException.h"

#include <semaphore.h>
#include <semaphore.h>
#include <thread>

namespace hycast {

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
class NodeImpl : public Node
{
    /**
     * Executes the peer-to-peer component. Blocks until `P2pMgr::run()` returns.
     */
    void runP2pMgr()
    {
        try {
            p2pMgr->run();
        }
        catch (const std::exception& ex) {
            setException(ex);
        }
    }

    /**
     * Executes the peer-to-peer component on a separate thread. Doesn't block.
     */
    void startP2pMgr() {
        try {
            p2pThread = Thread(&NodeImpl::runP2pMgr, this);
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't create thread to run P2P component"));
        }
    }

    /**
     * Stops the peer-to-peer component. Shouldn't block for long.
     */
    void stopP2pMgr() {
        if (p2pThread.joinable()) {
            p2pMgr->halt();
            p2pThread.join();
        }
    }

    /**
     * Executes the multicast component on a separate thread. Doesn't block.
     */
    void startSending() {
        try {
            mcastThread = Thread(&NodeImpl::runSending, this);
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't create thread to run multicast "
                    "component"));
        }
    }

    /**
     * Stops the multicast component. Shouldn't block for long.
     */
    void stopSending() {
        if (mcastThread.joinable()) {
            haltSending();
            mcastThread.join();
        }
    }

    /**
     * Executes the P2P and multicast components on separate threads. Doesn't block.
     */
    void startThreads() {
        try {
            startP2pMgr();
            startSending();
        }
        catch (const std::exception& ex) {
            stopThreads();
            throw;
        }
    }

    /**
     * Stops all sub-threads. Doesn't block.
     */
    void stopThreads() {
        stopSending();
        stopP2pMgr();
    }

protected:
    mutable Thread mcastThread;  ///< Multicast thread (either sending or receiving)
    mutable Thread p2pThread;    ///< Peer-to-peer thread (either publisher's or subscriber's)
    mutable sem_t  stopSem;      ///< For async-signal-safe stopping
    P2pMgr::Pimpl  p2pMgr;       ///< Peer-to-peer component (either publisher's or subscriber's)
    ThreadEx       threadEx;     ///< Subtask exception

    /**
     */
    void startImpl() {
        startThreads();
    }

    /**
     * Idempotent.
     */
    void stopImpl() {
        stopThreads();
        threadEx.throwIfSet();
    }

    /**
     * Sets the first exception thrown by a sub-thread.
     *
     * @param[in] ex  Sub-thread exception
     */
    void setException(const std::exception& ex) {
        threadEx.set(ex);
        ::sem_post(&stopSem);
    }

    /**
     * Executes the multicast component. Blocks until it returns.
     */
    virtual void runSending() =0;

    /**
     * Halts the multicast component. Doesn't block.
     */
    virtual void haltSending() =0;

public:
    /**
     * Constructs.
     *
     * @param[in] p2pMgr             Peer-to-peer manager
     * @throws    std::system_error  Couldn't initialize semaphore
     */
    NodeImpl(P2pMgr::Pimpl p2pMgr)
        : mcastThread()
        , p2pThread()
        , stopSem()
        , p2pMgr(p2pMgr)
        , threadEx()
    {
        if (::sem_init(&stopSem, 0, 0) == -1)
            throw SYSTEM_ERROR("Couldn't initialize semaphore");
    }

    virtual ~NodeImpl() noexcept {
        ::sem_destroy(&stopSem);
    }

    void run() override {
        startImpl();
        ::sem_wait(&stopSem);
        stopImpl();
        threadEx.throwIfSet();
    }

    void halt() override {
        int semval = 0;
        ::sem_getvalue(&stopSem, &semval);
        if (semval < 1)
            ::sem_post(&stopSem);
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
    McastPub::Pimpl    mcastPub;
    PubRepo            repo;
    SegSize            maxSegSize;

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

protected:
    /**
     * Multicasts new data-products in the repository and notifies peers.
     */
    void runSending()
    {
        // TODO: Make the priority of this thread less than the handling of missed-data requests
        try {
            for (;;) {
                auto prodInfo = repo.getNextProd();

#if 1
                // Send product-information
                send(prodInfo);

                /*
                 * Data-segment multicasting is interleaved with peer notification order to reduce
                 * the chance of losing a multicast data-segment by reducing the multicast rate.
                 */
                auto prodId = prodInfo.getId();
                auto prodSize = prodInfo.getSize();
                for (ProdSize offset = 0; offset < prodSize; offset += maxSegSize)
                    // TODO: Test for valid segment
                    send(repo.getDataSeg(DataSegId(prodId, offset)));
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
            setException(ex);
        }
        catch (...) {
            LOG_DEBUG("Thread cancelled");
            throw;
        }
    }

    void haltSending() {
        ::pthread_cancel(mcastThread.native_handle());
    }

public:
    /**
     * Constructs.
     *
     * @param[in] p2pAddr       Socket address of local P2P server. It shall specify a specific
     *                          interface and not the wildcard. The port number may be 0, in which case
     *                          the operating system will choose the port.
     * @param[in] maxPeers      Maximum number of P2P peers. It shall not be 0.
     * @param[in] evalTime      Evaluation interval for poorest-performing peer in seconds
     * @param[in] mcastAddr     Socket address of multicast group
     * @param[in] ifaceAddr     IP address of interface to use. If wildcard, then O/S chooses.
     * @param[in] listenSize    Size of `::listen()` queue
     * @param[in] repoRoot      Pathname of the root directory of the repository
     * @param[in] maxSegSize    Maximum size of a data-segment in bytes
     * @param[in] maxOpenFiles  Maximum number of open, data-products files
     * @throw InvalidArgument   `listenSize` is zero
     * @return                  New instance
     */
    PubNodeImpl(
            SockAddr       p2pAddr,
            const unsigned maxPeers,
            const unsigned evalTime,
            const SockAddr mcastAddr,
            const InetAddr ifaceAddr,
            const unsigned listenSize,
            const String&  repoRoot,
            const SegSize  maxSegSize,
            const long     maxOpenFiles)
        : NodeImpl(P2pMgr::create(*this, p2pAddr, maxPeers, listenSize, evalTime))
        , mcastPub(McastPub::create(mcastAddr, ifaceAddr))
        , repo(PubRepo(repoRoot, maxOpenFiles))
        , maxSegSize{maxSegSize}
    {}

    ~PubNodeImpl() noexcept {
        try {
            stopImpl();
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex, "Couldn't stop execution");
        }
    }

    SockAddr getP2pSrvrAddr() const override {
        return p2pMgr->getSrvrAddr();
    }

    void run() override {
        NodeImpl::run();
    }

    void halt() override {
        NodeImpl::halt();
    }

    void waitForPeer() override {
        NodeImpl::waitForPeer();
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

PubNode::Pimpl PubNode::create(
        SockAddr       p2pAddr,
        unsigned       maxPeers,
        const unsigned evalTime,
        const SockAddr mcastAddr,
        const InetAddr ifaceAddr,
        const unsigned listenSize,
        const String&  repoRoot,
        const SegSize  maxSegSize,
        const long     maxOpenFiles) {
    return Pimpl{new PubNodeImpl(p2pAddr, maxPeers, evalTime, mcastAddr, ifaceAddr,
            listenSize, repoRoot, maxSegSize, maxOpenFiles)};
}

PubNode::Pimpl PubNode::create(
        const SegSize            maxSegSize,
        const McastPub::RunPar&  mcastRunPar,
        const PubP2pMgr::RunPar& p2pRunPar,
        const PubRepo::RunPar&   repoRunPar) {
    return create(p2pRunPar.srvr.addr, p2pRunPar.maxPeers, p2pRunPar.evalTime, mcastRunPar.dstAddr,
            mcastRunPar.srcAddr, p2pRunPar.srvr.acceptQSize, repoRunPar.rootDir, maxSegSize,
            repoRunPar.maxOpenFiles);
}

/**************************************************************************************************/

/**
 * Implementation of a subscribing node.
 */
class SubNodeImpl final : public NodeImpl, public SubNode
{
    SubRepo                    repo;         ///< Data-product repository
    std::atomic<unsigned long> numMcastOrig; ///< Number of original multicast PDUs
    std::atomic<unsigned long> numP2pOrig;   ///< Number of original P2P PDUs
    std::atomic<unsigned long> numMcastDup;  ///< Number of duplicate multicast PDUs
    std::atomic<unsigned long> numP2pDup;    ///< Number of duplicate P2P PDUs
    McastSub::Pimpl            mcastSub;     ///< Multicast subscriber

    /**
     * Handles the publisher: Connects to it, reads the subscription information, sends the socket
     * address of the local P2P server, and closes the connection.
     *
     * @param[in]  pubAddr  Socket address of the publisher
     * @param[out] subInfo  Subscription information
     */
    void handlePublisher(
            const SockAddr pubAddr,
            SubInfo&       subInfo) {
        // Keep consonant with `Publisher::handleSubscriber()`
        Xprt xprt{TcpClntSock(pubAddr)}; // Default timeout
        subInfo.read(xprt);
        p2pMgr->getSrvrAddr().write(xprt);
    }

protected:
    void runSending()
    {
        try {
            mcastSub->run();
        }
        catch (const std::exception& ex) {
            setException(ex);
        }
    }

    void haltSending() {
        mcastSub->halt();
    }

public:
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
     */

    /**
     * Constructs.
     *
     * @param[in] subInfo       Subscription information
     * @param[in] mcastIface    IP address of interface to receive multicast on
     * @param[in] p2pSrvrAddr   Socket address for local P2P server. IP address must not be the
     *                          wildcard. If the port number is zero, then the O/S will choose an
     *                          ephemeral port number.
     * @param[in] acceptQSize   Size of `RpcSrvr::accept()` queue. Don't use 0.
     * @param[in] maxPeers      Maximum number of peers. Must not be zero. Might be adjusted.
     * @param[in] evalTime      Evaluation interval for poorest-performing peer in seconds
     * @param[in] repoDir       Pathname of root directory of data-product repository
     * @param[in] maxSegSize    Maximum size of a data-segment in bytes
     * @param[in] maxOpenFiles  Maximum number of open files in repository
     * @throw     LogicError    IP address families of multicast group address and multicast
     *                          interface don't match
     */
    SubNodeImpl(
            SubInfo&          subInfo,
            const InetAddr    mcastIface,
            const SockAddr    p2pSrvrAddr,
            const int         acceptQSize,
            const int         timeout,
            const unsigned    maxPeers,
            const unsigned    evalTime,
            const String&     repoDir,
            const long        maxOpenFiles)
        : NodeImpl(SubP2pMgr::create(*this, subInfo.tracker, p2pSrvrAddr, acceptQSize, timeout,
                maxPeers, evalTime))
        , repo(SubRepo(repoDir, subInfo.maxSegSize, maxOpenFiles))
        , numMcastOrig{0}
        , numP2pOrig{0}
        , numMcastDup{0}
        , numP2pDup{0}
        , mcastSub{McastSub::create(subInfo.mcast.dstAddr, subInfo.mcast.srcAddr, mcastIface, *this)}
    {}

    ~SubNodeImpl() noexcept {
        try {
            stopImpl();
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

    void run() override {
        NodeImpl::run();
    }

    void halt() override {
        NodeImpl::halt();
    }

    void waitForPeer() override {
        NodeImpl::waitForPeer();
    }

    ProdIdSet subtract(ProdIdSet rhs) const override {
        return repo.subtract(rhs);
    }

    ProdIdSet getProdIds() const override {
        return repo.getProdIds();
    }

    SockAddr getP2pSrvrAddr() const override {
        return p2pMgr->getSrvrAddr();
    }

    /**
     * Indicates if information on a product should be requested from the P2P network. Called by the
     * peer-to-peer component.
     *
     * @param[in] prodId     Identifier of product
     * @retval    `false`    Information shouldn't be requested
     * @retval    `true`     Information should be requested
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
     * @retval    `false`    Data-segment shouldn't be requested
     * @retval    `true`     Data-segment should be requested
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
     * @param[in] udpSeg   Multicast data-segment
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
     * @param[in] tcpSeg   Unicast data-segment
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

    ProdInfo getNextProd() override {
        return repo.getNextProd();
    }

    DataSeg getDataSeg(const DataSegId segId) override {
        return repo.getDataSeg(segId);
    }
};

/*
SubNode::Pimpl SubNode::create(
            SubInfo&          subInfo,
            const InetAddr    mcastIface,
            const TcpSrvrSock srvrSock,
            const int         acceptQSize,
            const int         timeout,
            const unsigned    maxPeers,
            const unsigned    evalTime,
            const String&     repoDir,
            const long        maxOpenFiles) {
    return Pimpl{new SubNodeImpl(subInfo, mcastIface, srvrSock, acceptQSize, timeout, maxPeers,
            evalTime, repoDir, maxOpenFiles)};
}
*/

SubNode::Pimpl SubNode::create(
            SubInfo&          subInfo,
            const InetAddr    mcastIface,
            const SockAddr    p2pSrvrAddr,
            const int         acceptQSize,
            const int         timeout,
            const unsigned    maxPeers,
            const unsigned    evalTime,
            const String&     repoDir,
            const long        maxOpenFiles) {
    return Pimpl{new SubNodeImpl(subInfo, mcastIface, p2pSrvrAddr, acceptQSize, timeout, maxPeers,
            evalTime, repoDir, maxOpenFiles)};
}

} // namespace
