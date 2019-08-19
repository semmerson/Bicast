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
 * Current P2P Protocol:
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
#include "PeerSet.h"
#include "PeerSetServer.h"
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
     * Manages active remote peers, which request missed chunks-of-data.
     */
    class P2pSender final
    {
        /**
         * Higher-level component that serves the set of active peers.
         */
        class PeerSetSrvr : public PeerSetServer
        {
            ProdStore         prodStore;
            static RecvStatus nilRecvStatus;

        public:
            /**
             * Constructs.
             * @param[in] ProdStore  Store of data-products
             * @param[in] peerSet    Set of active remote peers
             */
            PeerSetSrvr(ProdStore& prodStore)
                : prodStore{prodStore}
            {}

            /**
             * Returns the ID of the earliest missing chunk-of-data.
             * @return ID of earliest missing data-chunk
             */
            ChunkId getEarliestMissingChunkId()
            {
                return ChunkId{};
            }

            Backlogger getBacklogger(
                    const ChunkId& earliest,
                    PeerMsgSndr&          peer)
            {
                return Backlogger{peer, earliest, prodStore};
            }

            /// Do nothing because this node is the source
            void peerStopped(const InetSockAddr& peerAddr)
            {}

            /**
             * Receives product information from a peer.
             * @param[in] prodInfo  Product information
             * @param[in] peerAddr  Address of remote peer
             */
            RecvStatus receive(
                    const ProdInfo&     prodInfo,
                    const InetSockAddr& peerAddr)
            {
                return nilRecvStatus;
            }

            /**
             * Receives a chunk-of-data from a peer.
             * @param[in] chunk     Data-chunk
             * @param[in] peerAddr  Address of remote peer
             */
            RecvStatus receive(
                    LatentChunk&        chunk,
                    const InetSockAddr& peerAddr)
            {
                return nilRecvStatus;
            }

            bool shouldRequest(const ProdIndex& prodIndex)
            {
                return false;
            }

            /**
             * Indicates if a given chunk of data should be requested.
             * @param[in] id     Chunk identifier
             * @retval `true`    Chunk should be requested
             * @retval `false`   Chunk should not be requested
             * @exceptionsafety  Basic guarantee
             * @threadsafety     Safe
             */
            bool shouldRequest(const ChunkId& id)
            {
                return false;
            }

            bool get(
                    const ProdIndex& prodIndex,
                    ProdInfo&        prodInfo)
            {
                prodInfo = prodStore.getProdInfo(prodIndex);
                return prodInfo.operator bool();
            }

            bool get(
                    const ChunkId& id,
                    ActualChunk&   chunk)
            {
                chunk = prodStore.getChunk(id);
                return chunk.operator bool();
            }
        };

        Cue             serverReady; // Must initialize before `serverThread`
        PeerSetSrvr     peerSetSrvr; // Must initialize before `peerSet`
        PeerSet         peerSet;     // Must initialize after `peerSetSrvr`
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
                auto peer = PeerMsgSndr(sock);
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
                SrvrSctpSock serverSock{serverAddr, PeerMsgSndr::getNumStreams()};
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
         * @param[in] prodStore       Store of data-products
         * @param[in] serverAddr      Socket address of local server that
         *                            listens for connections from remote peers
         * @param[in] maxPeers        Maximum number of active peers
         * @param[in] stasisDuration  Minimum amount of time, in seconds, that
         *                            the set of active peers must be full and
         *                            unchanged before the worst-performing peer
         *                            may be removed
         */
        P2pSender(
                ProdStore&              prodStore,
                const InetSockAddr&     serverAddr,
                const unsigned          maxPeers,
                const unsigned          stasisDuration)
            : serverReady{}
            , peerSetSrvr{prodStore}
            , peerSet{peerSetSrvr, maxPeers, stasisDuration}
            , serverAddr{serverAddr}
            , serverThread{[this]{runServer();}}
        {
            serverReady.wait();
        }

        ~P2pSender()
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
            peerSet.notify(prodInfo.getIndex());
            ChunkIndex numChunks = prodInfo.getNumChunks();
            for (ChunkIndex i = 0; i < numChunks; ++i)
                peerSet.notify(ChunkId{prodInfo, i});
        }
    }; // Class `PeerMgr`

    ProdStore   prodStore;
    P2pSender   p2pSender;
    McastSender mcastSender;

public:
    /**
     * Constructs. Blocks until ready to accept an incoming connection from a
     * remote peer.
     * @param[in] prodStore       Product store
     * @param[in] mcastAddr       Multicast group socket address
     * @param[in] version         Protocol version
     * @param[in] stasisDuration  Minimum amount of time, in seconds, over which
     *                            the set of active peers must be unchanged
     *                            before the worst-performing peer may be
     *                            removed
     * @param[in] serverAddr      Socket address of local server for remote
     *                            peers
     */
    Impl(   ProdStore&              prodStore,
            const InetSockAddr&     mcastAddr,
            unsigned                version,
            const InetSockAddr&     serverAddr,
            unsigned                maxPeers,
            const unsigned          stasisDuration)
        : prodStore{prodStore}
        , p2pSender{prodStore, serverAddr, maxPeers, stasisDuration}
        , mcastSender{mcastAddr, version}
    {}

    /**
     * Ships a product.
     * @param[in] prod  Product to ship
     */
    void ship(Product& prod)
    {
        mcastSender.send(prod);
        // Following order is necessary
        prodStore.add(prod);
        p2pSender.notify(prod);
    }
}; // `Shipping::Impl`

RecvStatus Shipping::Impl::P2pSender::PeerSetSrvr::nilRecvStatus;

Shipping::Shipping(
        ProdStore&              prodStore,
        const InetSockAddr&     mcastAddr,
        const unsigned          version,
        const InetSockAddr&     serverAddr,
        const unsigned          maxPeers,
        const unsigned          stasisDuration)
    : pImpl{new Impl(prodStore, mcastAddr, version, serverAddr, maxPeers,
            stasisDuration)}
{}

void Shipping::ship(Product& prod)
{
    pImpl->ship(prod);
}

} // namespace
