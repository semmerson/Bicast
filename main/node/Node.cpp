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

#include "error.h"
#include "HycastProto.h"
#include "mcast.h"
#include "P2pMgr.h"
#include "Repository.h"
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
    void startMcast() {
        try {
            mcastThread = Thread(&NodeImpl::runMcast, this);
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(RUNTIME_ERROR("Couldn't create thread to run multicast "
                    "component"));
        }
    }

    /**
     * Stops the multicast component. Shouldn't block for long.
     */
    void stopMcast() {
        if (mcastThread.joinable()) {
            haltMcast();
            mcastThread.join();
        }
    }

    /**
     * Executes the P2P and multicast components on separate threads. Doesn't block.
     */
    void startThreads() {
        try {
            startP2pMgr();
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
    void stopThreads() {
        stopMcast();
        stopP2pMgr();
    }

protected:
    enum class State {
        INIT,
        STARTED,
        STOPPING,
        STOPPED
    }             state;        ///< State of this instance
    Mutex         stateMutex;   ///< For state changes
    Cond          stateCond;    ///< For state changes
    Thread        mcastThread;  ///< Multicast thread (either sending or receiving)
    Thread        p2pThread;    ///< Peer-to-peer thread (either publisher's or subscriber's)
    P2pMgr::Pimpl p2pMgr;       ///< Peer-to-peer component (either publisher's or subscriber's)
    ThreadEx      threadEx;     ///< Subtask exception

    /**
     * @pre               The state mutex is locked
     * @post              The state is STARTED
     * @throw LogicError  Instance can't be restarted
     */
    void startImpl() {
        if (state != State::INIT)
            throw LOGIC_ERROR("Instance can't be restarted");
        startThreads();
        state = State::STARTED;
    }

    /**
     * Idempotent.
     *
     * @pre   The state mutex is locked
     * @post  The state is STOPPED
     */
    void stopImpl() {
        stopThreads();
        state = State::STOPPED;
        threadEx.throwIfSet();
    }

    /**
     * Sets the first exception thrown by a sub-thread. Sets the state to STOPPING.
     *
     * @param[in] ex  Sub-thread exception
     */
    void setException(const std::exception& ex) {
        Guard guard{stateMutex};
        threadEx.set(ex);
        state = State::STOPPING;
        stateCond.notify_one();
    }

    /**
     * Executes the multicast component. Blocks until it returns.
     */
    virtual void runMcast() =0;

    /**
     * Halts the multicast component. Doesn't block.
     */
    virtual void haltMcast() =0;

public:
    /**
     * Constructs.
     *
     * @param[in] p2pMgr             Peer-to-peer manager
     * @throws    std::system_error  Couldn't initialize semaphore
     */
    NodeImpl(P2pMgr::Pimpl p2pMgr)
        : state(State::INIT)
        , stateMutex()
        , stateCond()
        , mcastThread()
        , p2pThread()
        , p2pMgr(p2pMgr)
        , threadEx()
    {}

    virtual ~NodeImpl() noexcept {
    }

    void run() override {
        Lock lock{stateMutex};
        startImpl();
        stateCond.wait(lock, [&]{return state == State::STOPPING;});
        stopImpl();
    }

    void halt() override {
        Guard guard{stateMutex};
        if (state == State::STARTED) {
            state = State::STOPPING;
            stateCond.notify_one();
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
            p2pMgr->notify(prodInfo.getIndex());
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
    void runMcast()
    {
        try {
            for (;;) {
                auto prodInfo = repo.getNextProd();

                // Send product-information
                send(prodInfo);

                /*
                 * Data-segment multicasting is interleaved with peer notification order to reduce
                 * the chance of losing a multicast data-segment by reducing the multicast rate.
                 */
                auto prodIndex = prodInfo.getIndex();
                auto prodSize = prodInfo.getSize();
                for (ProdSize offset = 0; offset < prodSize; offset += maxSegSize)
                    // TODO: Test for valid segment
                    send(repo.getDataSeg(DataSegId(prodIndex, offset)));
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

    void haltMcast() {
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
            unsigned       maxPeers,
            const SockAddr mcastAddr,
            const InetAddr ifaceAddr,
            const unsigned listenSize,
            const String&  repoRoot,
            const SegSize  maxSegSize,
            const long     maxOpenFiles)
        : NodeImpl(P2pMgr::create(*this, p2pAddr, maxPeers, listenSize))
        , mcastPub(McastPub::create(mcastAddr, ifaceAddr))
        , repo(PubRepo(repoRoot, maxSegSize, maxOpenFiles))
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

    /**
     * Links to a regular file or directory that's outside the repository. All regular files will be
     * published.
     *
     * @param[in] pathname       Absolute pathname (with no trailing '/') of the file or directory
     *                           to be linked to
     * @param[in] prodName       Product name if the pathname references a regular file and product
     *                           name prefix if the pathname references a directory
     * @throws LogicError        This instance doesn't support such linking
     * @throws InvalidArgument  `pathname` is empty or a relative pathname
     * @throws InvalidArgument  `prodName` is invalid
     */
    void link(
            const std::string& pathname,
            const std::string& prodName) override {
        repo.link(pathname, prodName);
    }

    /**
     * Receives a request for product information.
     *
     * @param[in] request      Which product
     * @return                 Requested product information. Will test false if it doesn't exist.
     */
    ProdInfo recvRequest(const ProdIndex request) override {
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
        const SockAddr mcastAddr,
        const InetAddr ifaceAddr,
        const unsigned listenSize,
        const String&  repoRoot,
        const SegSize  maxSegSize,
        const long     maxOpenFiles) {
    return Pimpl{new PubNodeImpl(p2pAddr, maxPeers, mcastAddr, ifaceAddr,
            listenSize, repoRoot, maxSegSize, maxOpenFiles)};
}

PubNode::Pimpl PubNode::create(
        const SegSize            maxSegSize,
        const McastPub::RunPar&  mcastRunPar,
        const PubP2pMgr::RunPar& p2pRunPar,
        const PubRepo::RunPar&   repoRunPar) {
    return create(p2pRunPar.srvr.addr, p2pRunPar.maxPeers, mcastRunPar.dstAddr, mcastRunPar.srcAddr,
            p2pRunPar.srvr.listenSize, repoRunPar.rootDir, maxSegSize, repoRunPar.maxOpenFiles);
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

protected:
    void runMcast()
    {
        try {
            mcastSub->run();
        }
        catch (const std::exception& ex) {
            setException(ex);
        }
    }

    void haltMcast() {
        mcastSub->halt();
    }

public:
    /**
     * Constructs.
     *
     * @param[in]     mcastAddr      Socket address of source-specific multicast group
     * @param[in]     srcAddr        IP address of source
     * @param[in]     ifaceAddr      IP address of interface to receive multicast on
     * @param[in]     p2pAddr        Socket address for local P2P server. It shall not specify the
     *                               wildcard. If port number is zero, then operating system will
     *                               choose an ephemeral port.
     * @param[in]     maxPeers       Maximum number of peers. Must not be zero. Might be adjusted.
     * @param[in,out] tracker        Pool of remote P2P-servers
     * @param[in,out] repo           Data-product repository
     */
    SubNodeImpl(
            const SockAddr mcastAddr,
            const InetAddr srcAddr,
            const InetAddr ifaceAddr,
            const SockAddr p2pAddr,
            unsigned       maxPeers,
            Tracker        tracker,
            const String&  repoRoot,
            const SegSize  maxSegSize,
            const long     maxOpenFiles)
        : NodeImpl(SubP2pMgr::create(*this, tracker, p2pAddr, maxPeers, maxSegSize))
        , repo(SubRepo(repoRoot, maxSegSize, maxOpenFiles))
        , numMcastOrig{0}
        , numP2pOrig{0}
        , numMcastDup{0}
        , numP2pDup{0}
        , mcastSub{McastSub::create(mcastAddr, srcAddr, ifaceAddr, *this)}
    {}

    ~SubNodeImpl() noexcept {
        try {
            stopImpl();
        }
        catch (const std::exception& ex) {
            LOG_ERROR(ex, "Couldn't stop execution");
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

    SockAddr getP2pSrvrAddr() const override {
        return p2pMgr->getSrvrAddr();
    }

    /**
     * Indicates if information on a product should be requested from the P2P
     * network. Called by the peer-to-peer component.
     *
     * @param[in] prodIndex  Identifier of product
     * @retval    `false`    Information shouldn't be requested
     * @retval    `true`     Information should be requested
     */
    bool shouldRequest(const ProdIndex prodIndex) override {
        //LOG_DEBUG("Determining if information on product %s should be requested",
                //prodIndex.to_string().data());
        return !repo.exists(prodIndex);
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
    ProdInfo recvRequest(const ProdIndex request) override {
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
        LOG_DEBUG("Saving product-information " + prodInfo.to_string());
        if (repo.save(prodInfo)) {
            ++numMcastOrig;
            p2pMgr->notify(prodInfo.getIndex());
        }
        else {
            ++numMcastDup;
        }
    }

    /**
     * Processes receipt of product information from the P2P network.
     *
     * @param[in] prodInfo  Product information
     */
    void recvP2pData(const ProdInfo prodInfo) override {
        LOG_DEBUG("Saving product-information " + prodInfo.to_string());
        repo.save(prodInfo) ? ++numP2pOrig : ++numP2pDup;
    }

    /**
     * Processes receipt of a data-segment from the multicast.
     *
     * @param[in] udpSeg   Multicast data-segment
     */
    void recvMcastData(const DataSeg dataSeg) override {
        LOG_DEBUG("Saving data-segment " + dataSeg.getId().to_string());
        if (repo.save(dataSeg)) {
            ++numMcastOrig;
            p2pMgr->notify(dataSeg.getId());
        }
        else {
            ++numMcastDup;
        }
    }

    /**
     * Processes receipt of a data-segment from the P2P network.
     *
     * @param[in] tcpSeg   Unicast data-segment
     */
    void recvP2pData(const DataSeg dataSeg) override {
        LOG_DEBUG("Saving data-segment " + dataSeg.getId().to_string());
        repo.save(dataSeg) ? ++numP2pOrig : ++numP2pDup;
    }

    ProdInfo getNextProd() override {
        return repo.getNextProd();
    }

    DataSeg getDataSeg(const DataSegId segId) override {
        return repo.getDataSeg(segId);
    }
};

SubNode::Pimpl SubNode::create(
            const SockAddr mcastAddr,
            const InetAddr srcAddr,
            const InetAddr ifaceAddr,
            const SockAddr p2pAddr,
            unsigned       maxPeers,
            Tracker        tracker,
            const String&  repoRoot,
            const SegSize  maxSegSize,
            const long     maxOpenFiles) {
    return Pimpl{new SubNodeImpl(mcastAddr, srcAddr, ifaceAddr, p2pAddr, maxPeers, tracker,
            repoRoot, maxSegSize, maxOpenFiles)};
}

} // namespace
