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

#include <node/Node.h>
#include "config.h"

#include "error.h"
#include "McastProto.h"
#include "P2pMgr.h"
#include "Repository.h"

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
    /**
     * Executes the P2P manager.
     */
    void runP2p()
    {
        try {
            p2pMgr();
        }
        catch (const std::exception& ex) {
            Guard guard(mutex);
            exPtr = std::make_exception_ptr(ex);
            cond.notify_one();
            // Not re-thrown so `std::terminate()` isn't called
        }
        catch (...) {
            // Cancellation eaten so `std::terminate()` isn't called
        }
    }

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
    Repository*        repo;         ///< Data-product repository
    ExceptPtr          exPtr;        ///< Subtask exception
    bool               done;         ///< Halt requested?
    P2pObs&            p2pObs;       ///< P2P network observer

    /**
     * Constructs.
     *
     * @param[in] p2pMgr  Peer-to-peer manager
     * @param[in] repo    Data-product repository
     */
    Impl(   P2pMgr&&    p2pMgr,
            Repository* repo,
            P2pObs&     p2pObs)
        : mutex()
        , cond()
        , p2pMgr(p2pMgr)
        , p2pMgrThread()
        , repo(repo)
        , exPtr()
        , done(false)
        , p2pObs(p2pObs)
    {}

    void handleException(const ExceptPtr& ptr)
    {
        Guard guard(mutex);

        if (!exPtr) {
            exPtr = ptr;
            cond.notify_one();
        }
    }

    void startP2pMgr() {
        p2pMgrThread = std::thread(&Impl::runP2p, this);
    }

    void stopP2pMgr() {
        if (p2pMgrThread.joinable()) {
            p2pMgr.halt();
            p2pMgrThread.join();
        }
    }

    void waitUntilDone()
    {
        Lock lock(mutex);

        while (!done && !exPtr)
            cond.wait(lock);
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

    void peerAdded(Peer peer) {
        p2pObs.peerAdded(peer);
    }

    ProdInfo getProdInfo(ProdIndex prodIndex)
    {
        return repo->getProdInfo(prodIndex);
    }

    MemSeg getMemSeg(const SegId& segId)
    {
        return repo->getMemSeg(segId);
    }
};

/******************************************************************************/

/**
 * Implementation of a publisher of data-products.
 */
class Publisher final : public Node::Impl
{
    McastSndr          mcastSndr;
    PubRepo            repo;
    SegSize            segSize;
    std::thread        sendThread;

    /**
     * Sends product-information.
     *
     * @param[in] prodInfo  Product-information to be sent
     */
    void send(const ProdInfo& prodInfo)
    {
        mcastSndr.multicast(prodInfo);
        p2pMgr.notify(prodInfo.getProdIndex());
    }

    /**
     * Sends a data-segment
     *
     * @param[in] memSeg  Data-segment to be sent
     */
    void send(const MemSeg& memSeg)
    {
        mcastSndr.multicast(memSeg);
        p2pMgr.notify(memSeg.getSegId());
    }

    /**
     * Executes the sender of new data-products in the repository.
     */
    void runSender()
    {
        try {
            for (;;) {
                auto prodIndex = repo.getNextProd();

                // Send product-information
                auto prodInfo = repo.getProdInfo(prodIndex);
                // TODO: Test for valid `prodInfo`
                send(prodInfo);

                // Send data-segments
                auto prodSize = prodInfo.getProdSize();
                for (ProdSize offset = 0; offset < prodSize; offset += segSize)
                    // TODO: Test for valid segment
                    send(repo.getMemSeg(SegId(prodIndex, offset)));
            }
        }
        catch (const std::exception& ex) {
            Guard guard(mutex);
            exPtr = std::make_exception_ptr(ex);
            cond.notify_one();
            // Not re-thrown so `std::terminate()` isn't called
        }
        catch (...) {
            // Cancellation eaten so `std::terminate()` isn't called
        }
    }

    void startSender() {
        sendThread = std::thread(&Publisher::runSender, this);
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
     * @param[in] repoDir  Pathname of root directory of repository
     * @param[in] p2pObs   Observer of the P2P network
     */
    Publisher(
            P2pInfo&           p2pInfo,
            const SockAddr&    grpAddr,
            const std::string& repoDir,
            P2pObs&            p2pObs)
        : Node::Impl(P2pMgr(p2pInfo, *this), &repo, p2pObs)
        , mcastSndr{UdpSock(grpAddr)}
        , repo(repoDir)
        , segSize{repo.getSegSize()}
        , sendThread()
    {
        mcastSndr.setMcastIface(p2pInfo.sockAddr.getInetAddr());
    }

    /**
     * Destroys.
     */
    ~Publisher() noexcept
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
    void operator()() {
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
            stopP2pMgr();
            throw;
        }

        if (!done && exPtr)
            std::rethrow_exception(exPtr);
    }
};

/******************************************************************************/

/**
 * Implementation of a subscriber of data-products.
 */
class Subscriber final : public Node::Impl, public P2pSub, public McastSub
{
    McastRcvr                  mcastRcvr;
    SubRepo                    repo;
    std::thread                mcastThread;
    std::atomic<unsigned long> numUdpOrig;
    std::atomic<unsigned long> numTcpOrig;
    std::atomic<unsigned long> numUdpDup;
    std::atomic<unsigned long> numTcpDup;

    void runMcast()
    {
        try {
            mcastRcvr();
        }
        catch (const std::exception& ex) {
            handleException(std::current_exception());
        }
    }

    void startMcastRcvr() {
        mcastThread = std::thread(&Subscriber::runMcast, this);
    }

    void stopMcastRcvr() {
        if (mcastThread.joinable()) {
            mcastRcvr.halt();
            mcastThread.join();
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
    Subscriber(
            const SrcMcastAddrs& srcMcastInfo,
            P2pInfo&             p2pInfo,
            ServerPool&          p2pSrvrPool,
            const std::string&   repoDir,
            const SegSize        segSize,
            P2pObs&              p2pObs)
        : Node::Impl(P2pMgr(p2pInfo, p2pSrvrPool, *this), &repo, p2pObs)
        , mcastRcvr{srcMcastInfo, *this}
        , repo(repoDir, segSize)
        , mcastThread{}
        , numUdpOrig{0}
        , numTcpOrig{0}
        , numUdpDup{0}
        , numTcpDup{0}
    {}

    /**
     * Executes this instance. Doesn't return until either `halt()` is called
     * or an exception is thrown.
     *
     * @see `halt()`
     */
    void operator()()
    {
        startMcastRcvr();

        try {
            startP2pMgr();

            try {
                waitUntilDone();

                LOG_NOTE("{original chunks: {UDP: %lu, TCP: %lu}, "
                        "duplicate chunks: {UDP: %lu, TCP: %lu}}",
                        numUdpOrig.load(), numTcpOrig.load(),
                        numUdpDup.load(), numTcpDup.load());

                stopP2pMgr();
            } // P2P manager started
            catch (const std::exception& ex) {
                stopP2pMgr();
                throw;
            }

            stopMcastRcvr();
        } // Multicast receiver started
        catch (const std::exception& ex) {
            stopMcastRcvr();
            throw;
        }

        if (!done && exPtr)
            std::rethrow_exception(exPtr);
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
    bool hereIs(UdpSeg& udpSeg)
    {
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
    bool hereIs(TcpSeg& tcpSeg)
    {
        const bool saved = repo.save(tcpSeg);
        saved ? ++numTcpOrig : ++numTcpDup;
        return saved;
    }
};

/******************************************************************************/

Node::Node(
        P2pInfo&           p2pInfo,
        const SockAddr&    grpAddr,
        const std::string& repoDir,
        P2pObs&            p2pObs)
    : pImpl{new Publisher(p2pInfo,  grpAddr, repoDir, p2pObs)} {
}

Node::Node(
        const SrcMcastAddrs& srcMcastInfo,
        P2pInfo&             p2pInfo,
        ServerPool&          p2pSrvrPool,
        const std::string&   repoDir,
        const SegSize        segSize,
        P2pObs&              p2pObs)
    : pImpl{new Subscriber(srcMcastInfo, p2pInfo, p2pSrvrPool, repoDir,
            segSize, p2pObs)} {
}

void Node::operator()() const {
    pImpl->operator()();
}

void Node::halt() const {
    pImpl->halt();
}

} // namespace
