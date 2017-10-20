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

#include "error.h"
#include "McastReceiver.h"
#include "P2pMgr.h"
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

class Receiving::Impl final : public McastMsgRcvr, public PeerMsgRcvr
{
    typedef std::mutex             Mutex;
    typedef std::lock_guard<Mutex> LockGuard;

    std::exception_ptr            exception;
    ProdStore                     prodStore;
    std::unordered_set<ChunkInfo> requestedChunks;
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
     * Indicates if a given chunk of data should be requested. If it should,
     * then the chunk's information is added to the set of requested chunks.
     * @param[in] info   Chunk information
     * @retval `true`    Chunk should be requested. Chunk information was added
     *                   to set of requested chunks.
     * @retval `false`   Chunk should not be requested
     * @exceptionsafety  Basic guarantee
     * @threadsafety     Safe
     */
    bool need(ChunkInfo info)
    {
        LockGuard lock(mutex);
        if (prodStore.haveChunk(info))
            return false;
        auto iter = requestedChunks.find(info);
        if (iter != requestedChunks.end())
            return false;
        requestedChunks.insert(info);
        return true;
    }

    /**
     * Accepts a chunk of data. Adds it to the product-store and erases it from
     * the set of outstanding chunk-requests. Processes the resulting product if
     * it's complete.
     * @param chunk      Chunk of data to accept
     * @exceptionsafety  Basic guarantee
     * @threadsafety     Safe
     */
    ProdStore::AddStatus accept(LatentChunk chunk)
    {
        LockGuard lock(mutex);
        requestedChunks.erase(chunk.getInfo());
        Product   prod;
        auto      status = prodStore.add(chunk, prod);
        if (!status.isDuplicate() && status.isComplete())
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
        , requestedChunks{}
        , mutex{}
        , processing{&processing}
        , p2pMgr{prodStore, p2pInfo, *this}
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
     * Receives a notice about a product via multicast. Adds the information to
     * the product-store. If the product is new, then a product-information
     * notice is sent to all peers. If the product is now complete, then it is
     * processed.
     * @param[in] prodInfo  Product information
     */
    void recvNotice(const ProdInfo& prodInfo)
    {
    	if (exception)
            std::rethrow_exception(exception);
    	if (!controlTraffic || trafficControler(generator)) {
            LOG_DEBUG("Received product-notice from multicast: prodInfo=%s",
                    prodInfo.to_string().c_str());
            Product prod;
            auto status = prodStore.add(prodInfo, prod);
            if (status.isComplete())
                processing->process(prod);
            if (status.isNew())
                p2pMgr.sendNotice(prodInfo);
    	}
    }

    /**
     * Receives a notice about a product from a peer. Adds the information to
     * the product-store. If the product is new, then a product-information
     * notice is sent to all peers. If the product is now complete, then it is
     * processed.
     * @param[in] prodInfo  Product information
     * @param[in] peer      Peer that received the information
     */
    void recvNotice(
            const ProdInfo& prodInfo,
            const Peer&     peer)
    {
    	if (exception)
            std::rethrow_exception(exception);
    	LOG_DEBUG("Received product-notice from peer: prodInfo=%s, peer=%s",
    	        prodInfo.to_string().c_str(),
    	        peer.getRemoteAddr().to_string().c_str());
        Product prod;
        auto status = prodStore.add(prodInfo, prod);
        if (!status.isDuplicate() && status.isComplete())
            processing->process(prod);
        if (status.isNew())
            p2pMgr.sendNotice(prodInfo);
    }

    /**
     * Receives a notice about a chunk of data from a peer. If the chunk hasn't
     * already been received, then it's information is added to the set of
     * requested chunks and the chunk is requested from the peer.
     * @param[in] info  Chunk information
     * @param[in] peer  Peer that received the chunk
     */
    void recvNotice(
            const ChunkInfo& info,
            const Peer&      peer)
    {
    	if (exception)
            std::rethrow_exception(exception);
    	LOG_DEBUG("Received chunk-notice from peer: chunkInfo=%s, peer=%s",
    	        info.to_string().c_str(),
    	        peer.getRemoteAddr().to_string().c_str());
        if (need(info))
            peer.sendRequest(info);
    }

    /**
     * Receives a request for information on a product from a peer. The peer is
     * sent the information if and only if the product-store contains it.
     * @param[in] index  Product index
     * @param[in] peer   Peer that made the request
     */
    void recvRequest(
            const ProdIndex& index,
            const Peer&      peer)
    {
    	if (exception)
            std::rethrow_exception(exception);
    	LOG_DEBUG("Received product-request from peer: prodIndex=%s, peer=%s",
    	        index.to_string().c_str(),
    	        peer.getRemoteAddr().to_string().c_str());
        ProdInfo info;
        if (prodStore.getProdInfo(index, info))
            peer.sendNotice(info);
    }

    /**
     * Receives a request for a chunk of data from a peer. The peer is
     * sent the chunk if and only if the product-store contains it.
     * @param[in] info   Chunk information
     * @param[in] peer   Peer that made the request
     */
    void recvRequest(
            const ChunkInfo& info,
            const Peer&      peer)
    {
    	if (exception)
            std::rethrow_exception(exception);
    	LOG_DEBUG("Received chunk-request from peer: chunkInfo=%s, peer=%s",
    	        info.to_string().c_str(),
    	        peer.getRemoteAddr().to_string().c_str());
        ActualChunk chunk;
        if (prodStore.getChunk(info, chunk))
            peer.sendData(chunk);
    }

    /**
     * Receives a chunk of data via multicast. A notice about the chunk is sent
     * to all peers.
     * @param[in] chunk  Chunk of data
     */
    void recvData(LatentChunk chunk)
    {
    	if (exception)
            std::rethrow_exception(exception);
    	if (controlTraffic && !trafficControler(generator)) {
    	    chunk.discard();
    	}
    	else {
            LOG_DEBUG("Received chunk via multicast: chunkInfo=%s",
                    chunk.getInfo().to_string().c_str());
            if (accept(chunk).isNew())
                p2pMgr.sendNotice(chunk.getInfo());
    	}
    }

    /**
     * Receives a chunk of data from a peer. A notice about the chunk is sent
     * to all other peers.
     * @param[in] chunk  Chunk of data
     * @param[in] peer   Peer that sent the chunk
     */
    void recvData(
            LatentChunk chunk,
            const Peer& peer)
    {
    	if (exception)
            std::rethrow_exception(exception);
    	LOG_DEBUG("Received chunk from peer: chunkInfo=%s, peer=%s",
    	        chunk.getInfo().to_string().c_str(),
    	        peer.getRemoteAddr().to_string().c_str());
        if (accept(chunk).isNew())
            p2pMgr.sendNotice(chunk.getInfo(), peer);
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

void Receiving::recvNotice(const ProdInfo& info)
{
    pImpl->recvNotice(info);
}

void Receiving::recvData(LatentChunk chunk)
{
    pImpl->recvData(chunk);
}

void Receiving::recvNotice(
        const ProdInfo& info,
        const Peer&     peer)
{
    pImpl->recvNotice(info, peer);
}

void Receiving::recvNotice(
        const ChunkInfo& info,
        const Peer&      peer)
{
    pImpl->recvNotice(info, peer);
}

void Receiving::recvRequest(
        const ProdIndex& index,
        const Peer&      peer)
{
    pImpl->recvRequest(index, peer);
}

void Receiving::recvRequest(
        const ChunkInfo& info,
        const Peer&      peer)
{
    pImpl->recvRequest(info, peer);
}

void Receiving::recvData(
        LatentChunk chunk,
        const Peer& peer)
{
    pImpl->recvData(chunk, peer);
}

} /* namespace hycast */
