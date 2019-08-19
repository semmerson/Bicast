/**
 * This file implements a component that coordinates the reception of
 * data-products. Data-products are received in pieces (product-information,
 * chunks of data) via both multicast and peer-to-peer transports.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Receiving.cpp
 * @author: Steven R. Emmerson
 */
#include "config.h"

#include "Backlogger.h"
#include "error.h"
#include "McastReceiver.h"
#include "P2pMgr.h"
#include "P2pMgrServer.h"
#include "ProdStore.h"
#include "Processing.h"
#include "Receiving.h"
#include "Thread.h"

#include <functional>
#include <mutex>
#include <pthread.h>
#include <random>
#include <thread>
#include <unordered_set>

namespace hycast {

class Receiving::Impl final : public McastContentRcvr
                            , public P2pMgrServer
{
    typedef std::mutex             Mutex;
    typedef std::lock_guard<Mutex> LockGuard;

    std::exception_ptr            exception;
    ProdStore                     prodStore;
    std::unordered_set<ProdIndex> requestedProdInfos;
    std::unordered_set<ChunkId>   requestedChunks;
    mutable Mutex                 mutex;
    Processing*                   processing;
    P2pMgr                        p2pMgr;
    Thread                        p2pMgrThread;
    McastReceiver                 mcastRcvr;
    Thread                        mcastRcvrThread;
    bool                          controlTraffic;
    std::default_random_engine    generator;
    std::bernoulli_distribution   trafficControler;

    /**
     * Throw the exception if it exists.
     * @pre `mutex` is locked
     */
    inline void throwIfException()
    {
    	if (exception)
            std::rethrow_exception(exception);
    }

    /**
     * Throw the exception if it exists.
     * @pre `mutex` is unlocked
     */
    inline void checkException()
    {
        LockGuard lock(mutex);
    	if (exception)
            std::rethrow_exception(exception);
    }

    /**
     * Runs the peer-to-peer manager. Intended to run on its own thread.
     */
    void runP2pMgr(Cue cue)
    {
    	try {
    	    cue.cue();
            p2pMgr.run();
            throw LOGIC_ERROR("Peer-to-peer manager stopped");
    	}
    	catch (const std::exception& e) {
            exception = std::current_exception();
    	}
    }

    /**
     * Runs the multicast receiver. Intended to run on its own thread.
     */
    void runMcastRcvr(Cue cue)
    {
    	try {
    	    cue.cue();
            mcastRcvr();
            throw LOGIC_ERROR("Multicast receiver stopped");
    	}
    	catch (const std::exception& e) {
            exception = std::current_exception();
    	}
    }

    /**
     * Accepts information on a product. Adds the information to the
     * product-store. If the associated product is now complete, then it is
     * processed.
     * @param[in] chunk  Data-chunk
     */
    RecvStatus accept(const ProdInfo& prodInfo)
    {
        PartialProduct prod{};
        RecvStatus     status{};
        {
            LockGuard lock{mutex};
            throwIfException();
            status = prodStore.add(prodInfo, prod);
            requestedProdInfos.erase(prodInfo.getIndex());
        }
        if (status.isNew() && status.isComplete())
            processing->process(prod);
        return status;
    }

    /**
     * Accepts a chunk-of-data. Adds the chunk to the product-store. If the
     * associated product is now complete, then it is processed.
     * @param[in] chunk  Data-chunk
     */
    RecvStatus accept(LatentChunk& chunk)
    {
        PartialProduct prod{};
        RecvStatus     status{};
        {
            LockGuard lock{mutex};
            throwIfException();
            status = prodStore.add(chunk, prod);
            requestedChunks.erase(chunk.getId());
        }
        if (status.isNew() && status.isComplete())
            processing->process(prod);
        return status;
    }

public:
    /**
     * Constructs.
     * @param[in] srcMcastInfo  Information on the source-specific multicast
     * @param[in] p2pInfo       Information for the peer-to-peer component
     * @param[in] pathname      Pathname of product-store persistence-file or
     *                          the empty string to indicate no persistence
     * @param[in] processing    Processes complete data-products. Must exist for
     *                          the duration of this instance.
     * @param[in] version       Protocol version
     * @param[in] drop          Proportion of multicast packets to drop. From
     *                          0 through 1, inclusive.
     * @see ProdStore::ProdStore()
     */
    Impl(   const SrcMcastInfo& srcMcastInfo,
            P2pInfo&            p2pInfo,
            const std::string&  pathname,
            Processing&         processing,
            const unsigned      version,
            const double        drop = 0)
        : exception{}
        , prodStore{pathname}
        , requestedProdInfos{}
        , requestedChunks{}
        , mutex{}
        , processing{&processing}
        , p2pMgr{p2pInfo, *this}
        , p2pMgrThread{}
        , mcastRcvr(srcMcastInfo, *this, version)
        , mcastRcvrThread{}
        , controlTraffic{drop > 0}
        , generator{}
    {
        Cue p2pMgrReady{};
        Cue mcastRcvrReady{};
        p2pMgrThread = Thread{&Impl::runP2pMgr, this, p2pMgrReady};
        mcastRcvrThread = Thread{&Impl::runMcastRcvr, this, mcastRcvrReady};
        if (controlTraffic)
            trafficControler = std::bernoulli_distribution{
                drop < 1 ? 1 - drop : 0};
        p2pMgrReady.wait();
        mcastRcvrReady.wait();
    }

    ~Impl()
    {
    	try {
            p2pMgrThread.cancel();
            p2pMgrThread.join();

            mcastRcvrThread.cancel();
            mcastRcvrThread.join();
    	}
    	catch (const std::exception& e) {
            log_error(e); // Because a destructor shouldn't throw an exception
    	}
    }

    /**
     * Returns the ID of the earliest missing chunk-of-data.
     * @return ID of earliest missing data-chunk
     */
    ChunkId getEarliestMissingChunkId()
    {
        return prodStore.getOldestMissingChunk();
    }

    Backlogger getBacklogger(const ChunkId& earliest, PeerMsgSndr& peer)
    {
        return Backlogger{peer, earliest, prodStore};
    }

    /**
     * Receives product information via multicast. Adds the information to the
     * product-store. If the information is new, then all active peers are
     * notified of its availability. If the associated product is now complete,
     * then it's processed.
     * @param[in] prodInfo  Product information
     */
    void receive(const ProdInfo& prodInfo)
    {
    	if (!controlTraffic || trafficControler(generator)) {
            auto status = accept(prodInfo);
            LOG_DEBUG("Received product-information " + prodInfo.to_string() +
                    " from multicast");
            if (status.isNew())
                p2pMgr.notify(prodInfo.getIndex());
    	}
    }

    /**
     * Receives product information from a peer. Adds the information to the
     * product-store if appropriate. If the product is now complete, then it is
     * processed.
     * @param[in] prodInfo  Product information
     * @param[in] peerAddr  Address of remote peer
     */
    RecvStatus receive(const ProdInfo& prodInfo, const InetSockAddr& peerAddr)
    {
        LOG_DEBUG("Received product-information " + prodInfo.to_string() +
                " from peer " + peerAddr.to_string());
        return accept(prodInfo);
    }

    /**
     * Receives a chunk-of-data via multicast. Adds the chunk to the product-
     * store if appropriate. If the product is now complete, then it is
     * processed. If the chunk is new, then all active peers are notified
     * of its availability.
     * @param[in] chunk  Data-chunk
     */
    void receive(LatentChunk chunk)
    {
    	if (controlTraffic && !trafficControler(generator)) {
    	    chunk.discard();
    	}
    	else {
            LOG_DEBUG("Received data-chunk " + chunk.getId().to_string() +
                    " from multicast");
            if (accept(chunk).isNew())
                p2pMgr.notify(chunk.getId());
    	}
    }

    /**
     * Receives a chunk-of-data from a peer. Adds the chunk to the product-
     * store if appropriate. If the product is now complete, then it's
     * processed.
     * @param[in] chunk     Data-chunk
     * @param[in] peerAddr  Address of remote peer
     */
    RecvStatus receive(LatentChunk& chunk, const InetSockAddr& peerAddr)
    {
        LOG_DEBUG("Received data-chunk " + chunk.getId().to_string() +
                " from peer " + peerAddr.to_string());
        return accept(chunk);
    }

    bool shouldRequest(const ProdIndex& prodIndex)
    {
        checkException();
        auto prodInfo = prodStore.getProdInfo(prodIndex);
        if (prodInfo.isComplete())
            return false;
        LockGuard lock{mutex};
        auto iter = requestedProdInfos.find(prodIndex);
        if (iter != requestedProdInfos.end())
            return false;
        requestedProdInfos.insert(prodIndex);
        return true;
    }

    /**
     * Indicates if a chunk-of-data should be requested. If it should be, then
     * the chunk's information is added to the set of requested chunks.
     * @param[in] id     Chunk identifier
     * @retval `true`    Chunk should be requested. Chunk information was added
     *                   to set of requested chunks.
     * @retval `false`   Chunk should not be requested
     * @exceptionsafety  Basic guarantee
     * @threadsafety     Safe
     */
    bool shouldRequest(const ChunkId& id)
    {
        checkException();
        if (prodStore.haveChunk(id))
            return false;
        LockGuard lock(mutex);
        auto iter = requestedChunks.find(id);
        if (iter != requestedChunks.end())
            return false;
        requestedChunks.insert(id);
        return true;
    }

    bool get(const ProdIndex& prodIndex, ProdInfo& prodInfo)
    {
        checkException();
        prodInfo = prodStore.getProdInfo(prodIndex);
        return prodInfo.operator bool();
    }

    bool get(const ChunkId& id, ActualChunk& chunk)
    {
        checkException();
        chunk = prodStore.getChunk(id);
        return chunk.operator bool();
    }
};

Receiving::Receiving(
        const SrcMcastInfo&  srcMcastInfo,
        P2pInfo&             p2pInfo,
        Processing&          processing,
        const unsigned       version,
        const std::string&   pathname,
        const double         drop)
    : pImpl{new Impl(srcMcastInfo, p2pInfo, pathname, processing, version,
            drop)}
{}

} /* namespace hycast */
