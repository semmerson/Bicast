/**
 * Nodes in the Hycast data-product distribution network.
 *
 *        File: Node.cpp
 *  Created on: Mar 9, 2020
 *      Author: Steven R. Emmerson
 *
 *    Copyright 2021 University Corporation for Atmospheric Research
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

#include <semaphore.h>
#include <thread>

namespace hycast {

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
            PeerSrvrAddrs peerSrvrAddrs;

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

/**
 * Base class for node implementations.
 */
class Node::Impl : public P2pSndr
{
protected:
    using Thread    = std::thread;
    using ExceptPtr = std::exception_ptr;

    sem_t              sem;          ///< Halt semaphore
    P2pMgr             p2pMgr;       ///< Peer-to-peer manager
    Thread             p2pMgrThread; ///< Thread on which P2P manager executes
    Repository&        repo;         ///< Data-product repository
    ExceptPtr          exPtr;        ///< Subtask exception

    /**
     * Constructs.
     *
     * @param[in] p2pMgr             Peer-to-peer manager
     * @param[in] repo               Data-product repository. Must exist for the
     *                               duration.
     * @throws    std::system_error  Couldn't initialize semaphore
     */
    Impl(   P2pMgr&&    p2pMgr,
            Repository& repo)
        : p2pMgr(p2pMgr)
        , p2pMgrThread()
        , repo(repo)
        , exPtr()
    {
        if (::sem_init(&sem, 0, 0) == -1)
            throw SYSTEM_ERROR("sem_init() failure");
    }

    void setException(const std::exception& ex)
    {
        if (!exPtr) {
            exPtr = std::make_exception_ptr(ex);
            if (::sem_post(&sem))
                throw SYSTEM_ERROR("sem_post() failure");
        }
    }

    /**
     * Waits until this instance should stop. Rethrows subtask exception if
     * appropriate.
     *
     * @throws std::system_error  `sem_wait()` failure
     * @throws                    Subtask exception if appropriate
     */
    void waitUntilDone()
    {
        int status;
        while ((status = sem_wait(&sem)) && errno == EINTR)
            continue;

        if (status)
            throw SYSTEM_ERROR("sem_wait() failure");

        if (exPtr)
            std::rethrow_exception(exPtr);
    }

    /**
     * @throw std::runtime_error  Couldn't create thread
     */
    void startP2pMgr() {
        try {
            p2pMgrThread = Thread(&Impl::runP2p, this);
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(
                    RUNTIME_ERROR("Couldn't create P2P manager thread"));
        }
    }

    void stopP2pMgr() {
        if (p2pMgrThread.joinable()) {
            p2pMgr.halt();
            p2pMgrThread.join();
        }
    }

public:
    /**
     * Destroys.
     */
    virtual ~Impl() noexcept {
        stopP2pMgr();
    }

    /**
     * Executes this instance.
     */
    virtual void operator()() =0;

    /**
     * Halts this instance.
     *
     * @asyncsignalsafety  Safe
     */
    void halt()
    {
        if (::sem_post(&sem))
            throw SYSTEM_ERROR("sem_post() failure");
    }

    ProdInfo getProdInfo(ProdIndex prodIndex)
    {
        return repo.getProdInfo(prodIndex);
    }

    MemSeg getMemSeg(const SegId& segId)
    {
        return repo.getMemSeg(segId);
    }

    /**
     * Links to a file (which could be a directory) that's outside the
     * repository. All regular files will be published.
     *
     * @param[in] pathname       Absolute pathname (with no trailing '/') of the
     *                           file or directory to be linked to
     * @param[in] prodName       Product name if the pathname references a file
     *                           and Product name prefix if the pathname
     *                           references a directory
     * @throws LogicError        This instance doesn't support such linking
     * @throws InvalidArgument  `pathname` is empty or a relative pathname
     * @throws InvalidArgument  `prodName` is invalid
     */
    virtual void link(
            const std::string& pathname,
            const std::string& prodName) =0;

private:
    /**
     * Executes the P2P manager.
     */
    void runP2p()
    {
        try {
            p2pMgr();
        }
        catch (const std::exception& ex) {
            setException(ex);
        }
        catch (...) {
            LOG_DEBUG("Thread cancelled");
            throw;
        }
    }
};

Node::Node()
    : pImpl{}
{}

Node::Node(Impl* impl)
    : pImpl{impl}
{}

Node::~Node() =default;

void Node::operator()() const {
    pImpl->operator()();
}

void Node::halt() const {
    pImpl->halt();
}

/******************************************************************************/

/**
 * Implementation of a publisher of data-products.
 */
class Publisher::Impl final : public Node::Impl
{
    McastSndr          mcastSndr;
    PubRepo            repo;
    SegSize            segSize;
    Thread             sendThread;

    /**
     * Sends product-information.
     *
     * @param[in] prodInfo      Product-information to be sent
     * @throws    RuntimeError  Couldn't send
     */
    void send(const ProdInfo& prodInfo)
    {
        try {
            mcastSndr.multicast(prodInfo);
            p2pMgr.notify(prodInfo.getProdIndex());
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
     * @param[in] memSeg        Data-segment to be sent
     * @throws    RuntimeError  Couldn't send
     */
    void send(const MemSeg& memSeg)
    {
        try {
            mcastSndr.multicast(memSeg);
            p2pMgr.notify(memSeg.getSegId());
        }
        catch (const std::exception& ex) {
            LOG_DEBUG("Exception thrown: %s", ex.what());
            std::throw_with_nested(RUNTIME_ERROR("Couldn't send data-segment " +
                    memSeg.to_string()));
        }
    }

    /**
     * Executes the sender of new data-products in the repository.
     */
    void runSender()
    {
        try {
            for (;;) {
                auto prodInfo = repo.getNextProd();

                // Send product-information
                send(prodInfo);

                // Send data-segments
                auto prodIndex = prodInfo.getProdIndex();
                auto prodSize = prodInfo.getProdSize();
                for (ProdSize offset = 0; offset < prodSize; offset += segSize)
                    // TODO: Test for valid segment
                    send(repo.getMemSeg(SegId(prodIndex, offset)));
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

    void startSender() {
        try {
            sendThread = Thread(&Impl::runSender, this);
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(
                    RUNTIME_ERROR("Couldn't create sending thread"));
        }
    }

    void stopSender() {
        if (sendThread.joinable()) {
            ::pthread_cancel(sendThread.native_handle());
            sendThread.join();
        }
    }

public:
    /**
     * Constructs.
     *
     * @param[in] p2pInfo  Information about the local P2P server
     * @param[in] grpAddr  Destination address for multicast products
     * @param[in] repo     Publisher's repository
     */
    Impl(   P2pInfo&        p2pInfo,
            const SockAddr& grpAddr,
            PubRepo&        repo)
        : Node::Impl(P2pMgr(p2pInfo, *this), repo)
        , mcastSndr{UdpSock(grpAddr)}
        , repo(repo)
        , segSize{repo.getSegSize()}
        , sendThread()
    {
        mcastSndr.setMcastIface(p2pInfo.sockAddr.getInetAddr());
    }

    /**
     * Destroys.
     */
    ~Impl()
    {
        if (sendThread.joinable()) {
            ::pthread_cancel(sendThread.native_handle());
            sendThread.join();
        }
        if (::sem_destroy(&sem))
            LOG_ERROR("sem_destroy() failure");
    }

    /**
     * Executes this instance. A P2P manager is executed and the repository is
     * watched for new data-products to be sent.
     *
     * @throw std::runtime_error  Couldn't create thread
     * @throw std::system_error  `sem_wait()` failure
     */
    void operator()() override {
        startP2pMgr();

        try {
            startSender();

            try {
                waitUntilDone();
                stopSender();
            } // Sender thread started
            catch (const std::exception& ex) {
                stopSender();
                throw;
            }

            stopP2pMgr();
        } // P2P manager started
        catch (const std::exception& ex) {
            LOG_DEBUG("Exception thrown: %s", ex.what());
            stopP2pMgr();
            throw;
        }
    }

    /**
     * Links to a file (which could be a directory) that's outside the
     * repository. All regular files will be published.
     *
     * @param[in] pathname       Absolute pathname (with no trailing '/') of the
     *                           file or directory to be linked to
     * @param[in] prodName       Product name if the pathname references a file
     *                           and Product name prefix if the pathname
     *                           references a directory
     * @throws LogicError        This instance doesn't support such linking
     * @throws InvalidArgument  `pathname` is empty or a relative pathname
     * @throws InvalidArgument  `prodName` is invalid
     */
    void link(
            const std::string& pathname,
            const std::string& prodName) {
        repo.link(pathname, prodName);
    }
};

Publisher::Publisher()
    : Node() {
}

Publisher::Publisher(
        P2pInfo&        p2pInfo,
        const SockAddr& grpAddr,
        PubRepo&        repo)
    : Node(new Impl{p2pInfo,  grpAddr, repo}) {
}

void Publisher::link(
        const std::string& pathname,
        const std::string& prodName) {
    pImpl->link(pathname, prodName);
}

#if 1
void Publisher::operator ()() const {
    pImpl->operator()();
}
#endif

/******************************************************************************/

/**
 * Implementation of a subscriber of data-products.
 */
class Subscriber::Impl final : public Node::Impl, public P2pSub, public McastSub
{
    McastRcvr                  mcastRcvr;       ///< Multicast receiver
    SubRepo                    repo;            ///< Data-product repository
    Thread                     mcastRcvrThread; ///< Multicast receiver thread
    std::atomic<unsigned long> numUdpOrig;      ///< Number of original UDP chunks
    std::atomic<unsigned long> numTcpOrig;      ///< Number of original TCP chunks
    std::atomic<unsigned long> numUdpDup;       ///< Number of duplicate UDP chunks
    std::atomic<unsigned long> numTcpDup;       ///< Number of duplicate TCP chunks

    void runMcast()
    {
        try {
            mcastRcvr();
        }
        catch (const std::exception& ex) {
            setException(ex);
        }
    }

    /**
     * @throw std::runtime_error  Couldn't create thread
     */
    void startMcastRcvr() {
        try {
            mcastRcvrThread = Thread(&Impl::runMcast, this);
        }
        catch (const std::exception& ex) {
            std::throw_with_nested(
                    RUNTIME_ERROR("Couldn't create multicast receiver thread"));
        }
    }

    void stopMcastRcvr() {
        if (mcastRcvrThread.joinable()) {
            mcastRcvr.halt();
            mcastRcvrThread.join();
        }
    }

public:
    /**
     * Constructs.
     *
     * @param[in]     srcMcastAddrs  Source-specific multicast addresses
     * @param[in,out] p2pInfo        Information about the local P2P server
     * @param[in,out] p2pSrvrPool    Pool of remote P2P-servers
     * @param[in,out] repo           Data-product repository
     * @param[in]     rcvrObs        Observer of this instance
     */
    Impl(   const SrcMcastAddrs& srcMcastAddrs,
            const P2pInfo&       p2pInfo,
            ServerPool&          p2pSrvrPool,
            SubRepo&             repo)
        : Node::Impl(P2pMgr(p2pInfo, p2pSrvrPool, *this), repo)
        , mcastRcvr{srcMcastAddrs, *this}
        , repo(repo)
        , mcastRcvrThread{}
        , numUdpOrig{0}
        , numTcpOrig{0}
        , numUdpDup{0}
        , numTcpDup{0}
    {}

    /**
     * Executes this instance. Doesn't return until either `halt()` is called
     * or an exception is thrown.
     *
     * @throw std::runtime_error  Couldn't create necessary thread
     * @see   `halt()`
     */
    void operator()() override {
        startMcastRcvr();

        try {
            startP2pMgr();

            try {
                waitUntilDone();

                LOG_NOTE("{original chunks: {UDP: %lu, TCP: %lu}, "
                        "duplicate chunks: {UDP: %lu, TCP: %lu}}",
                        numUdpOrig.load(), numTcpOrig.load(),
                        numUdpDup.load(), numTcpDup.load());

                stopP2pMgr(); // Idempotent
            } // P2P manager started
            catch (const std::exception& ex) {
                LOG_DEBUG("Exception thrown: %s", ex.what());
                stopP2pMgr(); // Idempotent
                throw;
            }

            stopMcastRcvr(); // Idempotent
        } // Multicast receiver started
        catch (const std::exception& ex) {
            LOG_DEBUG("Exception thrown: %s", ex.what());
            stopMcastRcvr(); // Idempotent
            throw;
        }
    }

    /**
     * Indicates if information on a product should be requested from the P2P
     * network. Called by the peer-to-peer manager.
     *
     * @param[in] prodIndex  Identifier of product
     * @retval    `false`    Information shouldn't be requested
     * @retval    `true`     Information should be requested
     */
    bool shouldRequest(ProdIndex prodIndex)
    {
        //LOG_DEBUG("Determining if information on product %s should be requested",
                //prodIndex.to_string().data());
        return !repo.exists(prodIndex);
    }

    /**
     * Indicates if a data-segment should be requested from the P2P network.
     * Called by the peer-to-peer manager.
     *
     * @param[in] segId      Identifier of data-segment
     * @retval    `false`    Data-segment shouldn't be requested
     * @retval    `true`     Data-segment should be requested
     */
    bool shouldRequest(const SegId& segId)
    {
        //LOG_DEBUG("Determining if data-segment %s should be requested",
                //segId.to_string().data());
        return !repo.exists(segId);
    }

    /**
     * Returns information on a product from the repository. Called by the P2P
     * manager.
     *
     * @param[in] prodIndex  Identifier of data-product
     * @return               Information on product. Will test false if no such
     *                       information exists.
     */
    ProdInfo getProdInfo(ProdIndex prodIndex)
    {
        return repo.getProdInfo(prodIndex);
    }

    /**
     * Returns a data-segment from the repository. Called by the P2P manager.
     *
     * @param[in] segId  Identifier of data-segment
     * @return           Data-segment. Will test false if no such segment
     *                   exists.
     */
    MemSeg getMemSeg(const SegId& segId)
    {
        return repo.getMemSeg(segId);
    }

    /**
     * Processes receipt of product information from the multicast.
     *
     * @param[in] prodInfo  Product information
     * @retval    `false`   Information is old
     * @retval    `true`    Information is new
     */
    bool hereIsMcast(const ProdInfo& prodInfo)
    {
        LOG_DEBUG("Saving product-information " + prodInfo.to_string());
        const bool saved = repo.save(prodInfo);
        if (saved) {
            ++numUdpOrig;
            p2pMgr.notify(prodInfo.getProdIndex());
        }
        else {
            ++numUdpDup;
        }
        return saved;
    }

    /**
     * Processes receipt of product information from the P2P network.
     *
     * @param[in] prodInfo  Product information
     * @retval    `false`   Information is old
     * @retval    `true`    Information is new
     */
    bool hereIsP2p(const ProdInfo& prodInfo)
    {
        LOG_DEBUG("Saving product-information " + prodInfo.to_string());
        const bool saved = repo.save(prodInfo);
        saved ? ++numTcpOrig : ++numTcpDup;
        return saved;
    }

    /**
     * Processes receipt of a data-segment from the multicast.
     *
     * @param[in] udpSeg   Multicast data-segment
     * @retval    `false`  Data-segment is old
     * @retval    `true`   Data-segment is new
     */
    bool hereIsMcast(UdpSeg& udpSeg)
    {
        LOG_DEBUG("Saving data-segment " + udpSeg.getSegId().to_string());
        const bool saved = repo.save(udpSeg);
        if (saved) {
            ++numUdpOrig;
            p2pMgr.notify(udpSeg.getSegId());
        }
        else {
            ++numUdpDup;
        }
        return saved;
    }

    /**
     * Processes receipt of a data-segment from the P2P network.
     *
     * @param[in] tcpSeg   Unicast data-segment
     * @retval    `false`  Data-segment is old
     * @retval    `true`   Data-segment is new
     */
    bool hereIsP2p(TcpSeg& tcpSeg)
    {
        LOG_DEBUG("Saving data-segment " + tcpSeg.getSegId().to_string());
        const bool saved = repo.save(tcpSeg);
        saved ? ++numTcpOrig : ++numTcpDup;
        return saved;
    }

    /**
     * Throws an exception.
     *
     * @throws LogicError        This instance doesn't support linking
     */
    void link(
            const std::string& pathname,
            const std::string& prodName) {
        throw LOGIC_ERROR("Unsupported operation");
    }
};

/******************************************************************************/

Subscriber::Subscriber(
        const SrcMcastAddrs& srcMcastAddrs,
        P2pInfo&             p2pInfo,
        ServerPool&          p2pSrvrPool,
        SubRepo&             repo)
    : Node{new Impl{srcMcastAddrs, p2pInfo, p2pSrvrPool, repo}} {
}

#if 0
void Subscriber::operator ()() const {
    pImpl->operator()();
}
#endif

} // namespace
