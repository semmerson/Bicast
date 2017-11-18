/**
 * This file implements a component that ships data-products to receiving nodes
 * using both multicast and peer-to-peer transports. It runs a server that
 * conditionally accepts connections from remote peers and manages a set of
 * active peers.
 *
 * Copyright 2017 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 * Current Protocol:
 *      Product Notice = ProdInfo
 *           ProdIndex       8
 *           ProdSize        4
 *           CanonChunkSize  2
 *           ProdName
 *              length       2
 *              char         ?
 *
 *      ProdInfo Request = ProdIndex
 *           ProdIndex       8
 *
 *      Chunk Notice = ChunkId
 *           ProdIndex       8
 *           ChunkIndex      4
 *
 *      Chunk Request = ChunkId:
 *
 *      Chunk = ActualChunk -> LatentChunk
 *           ChunkId        12
 *           ProdSize        4
 *           CanonChunkSize  2
 *           ChunkData
 *              ChunkSize    2 (transient; encoded in payload protocol ID)
 *              char         ?
 *
 *   @file: Shipping.cpp
 * @author: Steven R. Emmerson
 */
#include "config.h"

#include "error.h"
#include "PeerMsgRcvr.h"
#include "PeerSet.h"
#include "ProdStore.h"
#include "Shipping.h"
#include "SctpSock.h"
#include "Thread.h"

#include <pthread.h>
#include <thread>

namespace hycast {

class Shipping::Impl final
{
    /**
     * Manages active remote peers which request missed chunks-of-data.
     */
    class PeerMgr final
    {
        /**
         * Handles messages from remote peers. Only handles requests.
         */
        class MsgRcvr : public PeerMsgRcvr
        {
            ProdStore prodStore;

        public:
            /**
             * Constructs.
             * @param[in] ProdStore  Store of data-products
             * @param[in] peerSet    Set of active remote peers
             */
            MsgRcvr(ProdStore& prodStore)
                : prodStore{prodStore}
            {}

            /**
             * Receives a notice about a new product from a remote peer.
             * @param[in]     info  Information about the product
             * @param[in,out] peer  Peer that sent the notice
             */
            void recvNotice(const ProdInfo& info, const Peer& peer)
            {}

            /**
             * Receives a notice about a chunk-of-data from a remote peer.
             * @param[in]     info  Information about the chunk
             * @param[in,out] peer  Peer that sent the notice
             */
            void recvNotice(const ChunkId& info, const Peer& peer)
            {}

            /**
             * Receives a request for information about a product from a remote
             * peer.
             * @param[in]     index Index of the product
             * @param[in,out] peer  Peer that sent the request
             */
            void recvRequest(const ProdIndex& index, const Peer& peer)
            {
                auto info = prodStore.getProdInfo(index);
                if (info)
                    peer.sendNotice(info);
                //peerSet.decValue(peer); // Needy peers are bad?
            }

            /**
             * Receives a request for a chunk-of-data from a remote peer.
             * @param[in]     info  Information on the chunk
             * @param[in,out] peer  Peer that sent the request
             */
            void recvRequest(const ChunkId& id, const Peer& peer)
            {
                auto chunk = prodStore.getChunk(id);
                if (chunk)
                    peer.sendData(chunk);
                //peerSet.decValue(peer); // Needy peers are bad?
            }

            /**
             * Receives a chunk-of-data from a remote peer.
             * @param[in]     chunk  Chunk-of-data
             * @param[in,out] peer   Peer that sent the chunk
             */
            void recvData(LatentChunk chunk, const Peer& peer)
            {}
        };

        Cue             serverReady; // Must initialize before `serverThread`
        MsgRcvr         msgRcvr;     // Must be initialized before `peerSet`
        PeerSet         peerSet;     // Must be initialized after `msgRcvr`
        InetSockAddr    serverAddr;
        Thread          serverThread;

        /**
         * Accepts a connection from a remote peer. Tries to add it to the set
         * of active peers. Intended to run on its own thread.
         * @param[in] sock   Incoming connection
         * @exceptionsafety  Strong guarantee
         * @threadsafety     Safe
         */
        void accept(SctpSock sock)
        {
            try {
                // Blocks exchanging protocol version; hence, separate thread
                auto peer = Peer(sock);
                peerSet.tryInsert(peer);
            }
            catch (const std::exception& e) {
                log_warn(e); // Because end of thread
            }
        }

        /**
         * Runs the server for connections from remote peers. For each remote
         * peer that connects, a corresponding local peer is created and an
         * attempt is made to add it to the set of active peers. Doesn't return
         * unless an exception is thrown. Intended to be run on a separate
         * thread.
         * @exceptionsafety       Basic guarantee
         * @threadsafety          Compatible but not safe
         */
        void runServer()
        {
            try {
                SrvrSctpSock serverSock{serverAddr, Peer::getNumStreams()};
                serverSock.listen();
                for (;;) {
                    serverReady.cue();
                    auto sock = serverSock.accept(); // Blocks
                    std::thread([=]{accept(sock);}).detach();
                }
            }
            catch (const std::exception& e) {
                log_error(e); // Because end of thread
            }
        }

    public:
        /**
         * Constructs.
         * @param[in] prodStore   Store of data-products
         * @param[in] msgRcvr     Receiver of messages from remote peers
         * @param[in] peerSet     Initially empty set of active remote peers
         * @param[in] serverAddr  Socket address of local server that listens
         *                        for connections from remote peers
         */
        PeerMgr(ProdStore&              prodStore,
                const unsigned          maxPeers,
                const PeerSet::TimeUnit stasisDuration,
                const InetSockAddr&     serverAddr)
            : serverReady{}
            , msgRcvr{prodStore}
            , peerSet{prodStore, msgRcvr, stasisDuration*2, maxPeers,
                    [](InetSockAddr&){}}
            , serverAddr{serverAddr}
            , serverThread{[this]{runServer();}}
        {
            serverReady.wait();
        }

        ~PeerMgr()
        {
            try {
                try {
                    // Otherwise, server-socket won't close
                    serverThread.cancel();
                }
                catch (const std::exception& e) {
                    std::throw_with_nested(RUNTIME_ERROR(
                            "Couldn't destroy/join peer-server thread"));
                }
            }
            catch (const std::exception& e) {
                log_error(e); // Because destructors musn't throw
            }
        }

        /**
         * Notifies remote peers about the availability of a data-product.
         * @param[in] prod  Data-product
         */
        void notify(const Product& prod)
        {
            auto prodInfo = prod.getInfo();
            peerSet.sendNotice(prodInfo);
            ChunkIndex numChunks = prodInfo.getNumChunks();
            for (ChunkIndex i = 0; i < numChunks; ++i)
                peerSet.sendNotice(ChunkId{prodInfo, i});
        }
    }; // Class `PeerMgr`

    ProdStore   prodStore;
    PeerMgr     peerMgr;
    McastSender mcastSender;

public:
    /**
     * Constructs. Blocks until ready to accept an incoming connection from a
     * remote peer.
     * @param[in] prodStore       Product store
     * @param[in] mcastAddr       Multicast group socket address
     * @param[in] version         Protocol version
     * @param[in] stasisDuration  Duration over which the set of active peers
     *                            must be unchanged before the worst-performing
     *                            peer may be removed
     * @param[in] serverAddr      Socket address of local server for remote
     *                            peers
     */
    Impl(   ProdStore&              prodStore,
            const InetSockAddr&     mcastAddr,
            unsigned                version,
            unsigned                maxPeers,
            const PeerSet::TimeUnit stasisDuration,
            const InetSockAddr&     serverAddr)
        : prodStore{prodStore}
        , peerMgr{prodStore, maxPeers, stasisDuration, serverAddr}
        , mcastSender{mcastAddr, version}
    {}

    /**
     * Ships a product.
     * @param[in] prod  Product to ship
     */
    void ship(Product& prod)
    {
        // Order is important
        mcastSender.send(prod);
        prodStore.add(prod);
        peerMgr.notify(prod);
    }
}; // class Shipping::Impl

Shipping::Shipping(
        ProdStore&              prodStore,
        const InetSockAddr&     mcastAddr,
        const unsigned          version,
        const unsigned          maxPeers,
        const PeerSet::TimeUnit stasisDuration,
        const InetSockAddr&     serverAddr)
    : pImpl{new Impl(prodStore, mcastAddr, version, maxPeers, stasisDuration,
    		serverAddr)}
{}

void Shipping::ship(Product& prod)
{
    pImpl->ship(prod);
}

} // namespace
