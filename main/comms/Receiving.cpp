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

#include <functional>
#include <mutex>
#include <pthread.h>
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
    std::thread                   p2pMgrThread;
    McastReceiver                 mcastRcvr;
    std::thread                   mcastRcvrThread;

    /**
     * Runs the peer-to-peer manager. Intended to run on its own thread.
     */
    void runP2pMgr()
    {
    	try {
            p2pMgr();
            throw LogicError(__FILE__, __LINE__,
                    "Peer-to-peer manager stopped");
    	}
    	catch (const std::exception& e) {
            exception = std::current_exception();
    	}
    }

    /**
     * Runs the multicast receiver. Intended to run on its own thread.
     */
    void runMcastRcvr()
    {
    	try {
            mcastRcvr();
            throw LogicError(__FILE__, __LINE__, "Multicast receiver stopped");
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
     * the set of outstanding chunk-requests.
     * @param chunk      Chunk of data to accept
     * @exceptionsafety  Basic guarantee
     * @threadsafety     Safe
     */
    void accept(LatentChunk chunk)
    {
        LockGuard lock(mutex);
        requestedChunks.erase(chunk.getInfo());
        Product   prod;
        if (prodStore.add(chunk, prod))
        	processing->process(prod);
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
     * @see ProdStore::ProdStore()
     */
    Impl(   const SrcMcastInfo& srcMcastInfo,
            P2pInfo&            p2pInfo,
            const std::string&  pathname,
            Processing&         processing,
            const unsigned      version)
        : exception{}
        , prodStore{pathname}
        , requestedChunks{}
        , mutex{}
        , processing{&processing}
        , p2pMgr{p2pInfo, *this}
        //, p2pMgrThread{[this]{runP2pMgr();}}
        , mcastRcvr(srcMcastInfo, *this, version)
        , mcastRcvrThread{[this]{runMcastRcvr();}}
    {}

    ~Impl()
    {
    	try {
            int status = ::pthread_cancel(mcastRcvrThread.native_handle());
            if (status)
                throw SystemError(__FILE__, __LINE__,
                        "Couldn't cancel multicast receiver thread", status);
            mcastRcvrThread.join();

            /*
            status = ::pthread_cancel(p2pMgrThread.native_handle());
            if (status)
                throw SystemError(__FILE__, __LINE__,
                        "Couldn't cancel P2P manager thread", status);
            p2pMgrThread.join();
            */
    	}
    	catch (const std::exception& e) {
    		log_what(e); // Because a destructor shouldn't throw an exception
    	}
    }

    /**
     * Receives a notice about a product via multicast. Adds the information to
     * the product-store.
     * @param[in] info  Product information
     */
    void recvNotice(const ProdInfo& info)
    {
    	if (exception)
            std::rethrow_exception(exception);
        Product prod;
        if (prodStore.add(info, prod))
            processing->process(prod);
    }

    /**
     * Receives a notice about a product from a peer. Adds the information to
     * the product-store.
     * @param[in] info  Product information
     * @param[in] peer  Peer that received the information
     */
    void recvNotice(
            const ProdInfo& info,
            const Peer&     peer)
    {
    	if (exception)
            std::rethrow_exception(exception);
        Product prod;
        if (prodStore.add(info, prod))
            processing->process(prod);
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
        accept(chunk);
        p2pMgr.sendNotice(chunk.getInfo());
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
        accept(chunk);
        p2pMgr.sendNotice(chunk.getInfo(), peer);
    }
};

Receiving::Receiving(
        const SrcMcastInfo&  srcMcastInfo,
        P2pInfo&             p2pInfo,
        Processing&          processing,
        const unsigned       version,
        const std::string&   pathname)
    : pImpl{new Impl(srcMcastInfo, p2pInfo, pathname, processing, version)}
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
