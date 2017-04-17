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
 *   @file: Shipping.cpp
 * @author: Steven R. Emmerson
 */
#include "config.h"

#include "error.h"
#include "PeerMsgRcvr.h"
#include "PeerSet.h"
#include "ProdStore.h"
#include "Shipping.h"
#include "SrvrSctpSock.h"

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
    	 * Handles messages from remove peers. Only handles requests: doesn't
    	 * expect or handle notices or data-chunks.
    	 */
    	class MsgRcvr : public PeerMsgRcvr
		{
    		ProdStore prodStore;
    		PeerSet   peerSet;

		public:
    		/**
    		 * Constructs.
    		 * @param[in] ProdStore  Store of data-products
    		 * @param[in] peerSet    Set of active remote peers
    		 */
    		MsgRcvr(ProdStore& ProdStore,
    				PeerSet&   peerSet)
				: prodStore{prodStore}
    			, peerSet{peerSet}
    		{}

			/**
			 * Receives a notice about a new product from a remote peer.
			 * @param[in]     info  Information about the product
			 * @param[in,out] peer  Peer that sent the notice
			 */
			void recvNotice(const ProdInfo& info, Peer& peer)
			{}

			/**
			 * Receives a notice about a chunk-of-data from a remote peer.
			 * @param[in]     info  Information about the chunk
			 * @param[in,out] peer  Peer that sent the notice
			 */
			void recvNotice(const ChunkInfo& info, Peer& peer)
			{}

			/**
			 * Receives a request for information about a product from a remote
			 * peer.
			 * @param[in]     index Index of the product
			 * @param[in,out] peer  Peer that sent the request
			 */
			void recvRequest(const ProdIndex& index, Peer& peer)
			{
				ProdInfo info;
				if (prodStore.getProdInfo(index, info))
					peer.sendNotice(info);
				peerSet.decValue(peer); // Needy peers are bad
			}

			/**
			 * Receives a request for a chunk-of-data from a remote peer.
			 * @param[in]     info  Information on the chunk
			 * @param[in,out] peer  Peer that sent the request
			 */
			void recvRequest(const ChunkInfo& info, Peer& peer)
			{
				ActualChunk chunk;
				if (prodStore.getChunk(info, chunk))
					peer.sendData(chunk);
				peerSet.decValue(peer); // Needy peers are bad
			}

			/**
			 * Receives a chunk-of-data from a remote peer.
			 * @param[in]     chunk  Chunk-of-data
			 * @param[in,out] peer   Peer that sent the chunk
			 */
			void recvData(LatentChunk chunk, Peer& peer)
			{}
		};

    	MsgRcvr     msgRcvr;
        PeerSet     peerSet;
        std::thread serverThread;

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
				auto peer = Peer(msgRcvr, sock);
				peerSet.tryInsert(peer, nullptr);
			}
			catch (const std::exception& e) {
				log_what(e); // Because end of thread
			}
		}

		/**
		 * Runs the server for connections from remote peers. Creates a
		 * corresponding local peer and attempts to add it to the set of active
		 * peers. Doesn't return unless an exception is thrown. Intended to be
		 * run on a separate thread.
		 * @param[in] serverAddr  Socket address of local server for remote
		 *                        peers
		 * @exceptionsafety       Basic guarantee
		 * @threadsafety          Compatible but not safe
		 */
		void runServer(const InetSockAddr serverAddr)
		{
			try {
				SrvrSctpSock serverSock{serverAddr, Peer::getNumStreams()};
				for (;;) {
					auto sock = serverSock.accept(); // Blocks
					std::thread([=]{accept(sock);}).detach();
				}
			}
			catch (const std::exception& e) {
				log_what(e); // Because end of thread
			}
		}

    public:
		/**
		 * Constructs.
		 * @param[in] prodStore   Store of data-products
		 * @param[in] peerSet     Initially empty set of active remote peers
		 * @param[in] serverAddr  Socket address of local server that listens
		 *                        for connections from remote peers
		 */
        PeerMgr(ProdStore&          prodStore,
        		PeerSet&            peerSet,
        		const InetSockAddr& serverAddr)
            : msgRcvr{prodStore, peerSet}
        	, peerSet{peerSet}
        	, serverThread{[=]{runServer(serverAddr);}}
        {
        	serverThread.detach();
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
                peerSet.sendNotice(ChunkInfo{prodInfo, i});
        }
    };

    ProdStore   prodStore;
    PeerMgr     peerMgr;
    McastSender mcastSender;

public:
    /**
     * Constructs.
     * @param[in] prodStore    Product store
     * @param[in] mcastSender  Multicast sender.
     * @param[in] peerSet      Initially empty set of active peers
     * @param[in] serverAddr   Socket address of local server for remote peers
     */
    Impl(   ProdStore&          prodStore,
            McastSender&        mcastSender,
            PeerSet&            peerSet,
			const InetSockAddr& serverAddr)
        : prodStore{prodStore}
        , peerMgr{prodStore, peerSet, serverAddr}
        , mcastSender{mcastSender}
    {}

    /**
     * Ships a product.
     * @param[in] prod  Product to ship
     */
    void ship(Product& prod)
    {
        mcastSender.send(prod);
        prodStore.add(prod);
        peerMgr.notify(prod);
    }
};

Shipping::Shipping(
        ProdStore&          prodStore,
        McastSender&        mcastSender,
        PeerSet&            peerSet,
		const InetSockAddr& serverAddr)
    : pImpl{new Impl(prodStore, mcastSender, peerSet, serverAddr)}
{}

void Shipping::ship(Product& prod)
{
    pImpl->ship(prod);
}

} // namespace
