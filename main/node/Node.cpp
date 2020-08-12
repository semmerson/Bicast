/**
 * Nodes in the Hycast data-product distribution network.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Node.cpp
 *  Created on: Mar 9, 2020
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "Node.h"

#include "error.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace hycast {

/**
 * Base class for node implementations.
 */
class Node::Impl : public P2pSndr
{
protected:
    typedef std::mutex              Mutex;
    typedef std::condition_variable Cond;
    typedef std::lock_guard<Mutex>  Guard;
    typedef std::unique_lock<Mutex> Lock;
    typedef std::exception_ptr      ExceptPtr;

    Mutex              mutex;        ///< For concurrency
    Cond               cond;         ///< For concurrency
    P2pMgr             p2pMgr;       ///< Peer-to-peer manager
    std::thread        p2pMgrThread; ///< Thread on which P2P manager executes
    Repository&        repo;         ///< Data-product repository
    ExceptPtr          exPtr;        ///< Subtask exception
    bool               done;         ///< Halt requested?

    /**
     * Constructs.
     *
     * @param[in] p2pMgr  Peer-to-peer manager
     * @param[in] repo    Data-product repository. Must exist for the duration.
     */
    Impl(   P2pMgr&&    p2pMgr,
            Repository& repo)
        : mutex()
        , cond()
        , p2pMgr(p2pMgr)
        , p2pMgrThread()
        , repo(repo)
        , exPtr()
        , done(false)
    {}

    void setException(const std::exception& ex)
    {
        Guard guard(mutex);

        if (!exPtr) {
            exPtr = std::make_exception_ptr(ex);
            cond.notify_one();
        }
    }

    /**
     * Waits until this instance should stop. Rethrows subtask exception if
     * appropriate.
     */
    void waitUntilDone()
    {
        Lock lock(mutex);

        while (!done && !exPtr)
            cond.wait(lock);

        if (!done && exPtr)
            std::rethrow_exception(exPtr);
    }

    /**
     * @throw std::runtime_error  Couldn't create thread
     */
    void startP2pMgr() {
        try {
            p2pMgrThread = std::thread(&Impl::runP2p, this);
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
        Guard guard(mutex); // For visibility of changes
        stopP2pMgr();
    }

    /**
     * Executes this instance.
     */
    virtual void operator()() =0;

    /**
     * Halts this instance.
     */
    void halt()
    {
        Guard guard(mutex);
        done = true;
        cond.notify_one();
    }

    /*
    void peerAdded(Peer peer) {
        p2pObs.peerAdded(peer);
    }
    */

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

Node::Node(std::shared_ptr<Impl> pImpl)
    : pImpl{pImpl}
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
    std::thread        sendThread;

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
        sendThread = std::thread(&Impl::runSender, this);
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
        Guard guard(mutex);

        if (sendThread.joinable()) {
            ::pthread_cancel(sendThread.native_handle());
            sendThread.join();
        }
    }

    /**
     * Executes this instance. A P2P manager is executed and the repository is
     * watched for new data-products to be sent.
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

Publisher::Publisher(
        P2pInfo&        p2pInfo,
        const SockAddr& grpAddr,
        PubRepo&        repo)
    : Node{std::make_shared<Impl>(p2pInfo,  grpAddr, repo)} {
}

void Publisher::link(
        const std::string& pathname,
        const std::string& prodName) {
    pImpl->link(pathname, prodName);
}

#if 0
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
    std::thread                mcastRcvrThread; ///< Multicast receiver thread
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
            mcastRcvrThread = std::thread(&Impl::runMcast, this);
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
     * @param[in]     srcMcastInfo  Information on source-specific multicast
     * @param[in,out] p2pInfo       Information about the local P2P server
     * @param[in,out] p2pSrvrPool   Pool of remote P2P-servers
     * @param[in,out] repo          Data-product repository
     * @param[in]     rcvrObs       Observer of this instance
     */
    Impl(   const SrcMcastAddrs& srcMcastInfo,
            P2pInfo&             p2pInfo,
            ServerPool&          p2pSrvrPool,
            SubRepo&             repo)
        : Node::Impl(P2pMgr(p2pInfo, p2pSrvrPool, *this), repo)
        , mcastRcvr{srcMcastInfo, *this}
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
        const SrcMcastAddrs& srcMcastInfo,
        P2pInfo&             p2pInfo,
        ServerPool&          p2pSrvrPool,
        SubRepo&             repo)
    : Node{std::make_shared<Impl>(srcMcastInfo, p2pInfo, p2pSrvrPool, repo)} {
}

#if 0
void Subscriber::operator ()() const {
    pImpl->operator()();
}
#endif

} // namespace
