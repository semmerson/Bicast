/**
 * Subscriber node.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Subscriber.cpp
 *  Created on: Jan 13, 2020
 *      Author: Steven R. Emmerson
 */

#include <node/Subscriber.h>
#include "config.h"

#include "error.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace hycast {

class Subscriber::Impl : public McastSub, public RecvNode
{
    typedef std::mutex              Mutex;
    typedef std::lock_guard<Mutex>  Guard;
    typedef std::unique_lock<Mutex> Lock;
    typedef std::condition_variable Cond;
    typedef std::exception_ptr      ExceptPtr;

    mutable Mutex              doneMutex;
    mutable Cond               doneCond;
    McastRcvr                  mcastRcvr;
    SubP2pMgr                  p2pMgr;
    SubRepo                    repo;
    std::thread                mcastThread;
    std::thread                p2pThread;
    ExceptPtr                  exceptPtr;
    bool                       haltRequested;
    std::atomic<unsigned long> numMcastOrig;
    std::atomic<unsigned long> numUcastOrig;
    std::atomic<unsigned long> numMcastDup;
    std::atomic<unsigned long> numUcastDup;

    void handleException(const ExceptPtr& exPtr)
    {
        Guard guard(doneMutex);

        if (!exceptPtr) {
            exceptPtr = exPtr;
            doneCond.notify_one();
        }
    }

    void runMcast()
    {
        try {
            mcastRcvr();
        }
        catch (const std::exception& ex) {
            handleException(std::current_exception());
        }
    }

    void runP2p()
    {
        try {
            p2pMgr();
        }
        catch (const std::exception& ex) {
            handleException(std::current_exception());
        }
    }

    void waitUntilDone()
    {
        Lock lock(doneMutex);

        while (!haltRequested && !exceptPtr)
            doneCond.wait(lock);
    }

public:
    /**
     * Constructs.
     *
     * @param[in]     srcMcastInfo  Information on source-specific multicast
     * @param[in,out] p2pInfo       Information on peer-to-peer network
     * @param[in,out] p2pSrvrPool   Pool of potential peer-servers
     * @param[in,out] repo          Data-product repository
     * @param[in]     rcvrObs       Observer of this instance
     */
    Impl(   const SrcMcastAddrs& srcMcastInfo,
            P2pInfo&             p2pInfo,
            ServerPool&          p2pSrvrPool,
            SubRepo&             repo)
        : mcastRcvr{srcMcastInfo, *this}
        , p2pMgr{p2pInfo, p2pSrvrPool, *this}
        , repo{repo}
        , mcastThread{}
        , p2pThread{}
        , exceptPtr{}
        , haltRequested{false}
        , numMcastOrig{0}
        , numUcastOrig{0}
        , numMcastDup{0}
        , numUcastDup{0}
    {}

    SendNode& asSendNode() noexcept {
        return *this;
    }

    /**
     * Executes this instance. Doesn't return until either `halt()` is called
     * or an exception is thrown.
     *
     * @see `halt()`
     */
    void operator()()
    {
        mcastThread = std::thread(&Impl::runMcast, this);

        try {
            p2pThread = std::thread(&Impl::runP2p, this);

            try {
                waitUntilDone();

                {
                    Guard guard{doneMutex};
                    if (!haltRequested && exceptPtr)
                        std::rethrow_exception(exceptPtr);
                }

                p2pThread.join();
                LOG_NOTE("{original: {mcast: %lu, ucast: %lu}, "
                        "duplicate: {mcast: %lu, ucast: %lu}}",
                        numMcastOrig.load(), numUcastOrig.load(),
                        numMcastDup.load(), numUcastDup.load());
            }
            catch (const std::exception& ex) {
                p2pMgr.halt();
                p2pThread.join();
                throw;
            }

            mcastThread.join();
        }
        catch (const std::exception& ex) {
            mcastRcvr.halt();
            mcastThread.join();
            throw;
        }
    }

    /**
     * Halts execution of this instance. Causes `operator()()` to return.
     *
     * @see `operator()()`
     */
    void halt()
    {
        p2pMgr.halt();
        mcastRcvr.halt();

        Guard guard{doneMutex};
        haltRequested = true;
        doneCond.notify_one();
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
    ProdInfo get(ProdIndex prodIndex)
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
    MemSeg get(const SegId& segId)
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
            ++numMcastOrig;
            p2pMgr.notify(prodInfo.getProdIndex());
        }
        else {
            ++numMcastDup;
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
        saved ? ++numUcastOrig : ++numUcastDup;
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
            ++numMcastOrig;
            p2pMgr.notify(udpSeg.getSegId());
        }
        else {
            ++numMcastDup;
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
        saved ? ++numUcastOrig : ++numUcastDup;
        return saved;
    }
};

/******************************************************************************/

Subscriber::Subscriber(
            const SrcMcastAddrs& srcMcastInfo,
            P2pInfo&            p2pInfo,
            ServerPool&         p2pSrvrPool,
            SubRepo&            repo)
    : pImpl{new Impl(srcMcastInfo, p2pInfo, p2pSrvrPool, repo)} {
}

void Subscriber::operator()() const {
    pImpl->operator()();
}

void Subscriber::halt() const {
    pImpl->halt();
}

} // namespace
