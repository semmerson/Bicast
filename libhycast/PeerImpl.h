/**
 * This file declares a connection between peers.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerConnection.h
 * @author: Steven R. Emmerson
 */

#ifndef PEERIMPL_H_
#define PEERIMPL_H_

#include "ChunkChannel.h"
#include "ChunkInfo.h"
#include "Peer.h"
#include "ProdIndex.h"
#include "ProdInfo.h"
#include "RegChannel.h"
#include "Socket.h"
#include "VersionMsg.h"

#include <condition_variable>
#include <mutex>
#include <thread>

namespace hycast {

class PeerImpl final {
    template<class T> class ObjQueue {
        std::mutex              mutex;
        std::condition_variable cond;
        bool                    haveObj;
        T                       obj;
    public:
        ObjQueue();
        void put(T& obj);
        T    get();
    };
    class RecvQueue {
        static const unsigned   nelt = MSGTYPE_NUM_TYPES + 1;
        mutable std::mutex      mutex;
        std::condition_variable cond;
        MsgType                 circBuf[nelt];
        unsigned                nextPut;
        unsigned                nextGet;
    public:
        RecvQueue();
        void    put(MsgType type);
        MsgType get();
        bool    empty() const;
    };
    typedef enum {
        VERSION_STREAM_ID = 0,
        PROD_NOTICE_STREAM_ID,
        CHUNK_NOTICE_STREAM_ID,
        PROD_REQ_STREAM_ID,
        CHUNK_REQ_STREAM_ID,
        CHUNK_STREAM_ID,
        NUM_STREAM_IDS
    }      SctpStreamId;
    unsigned               version;
    RegChannel<VersionMsg> versionChan;
    RegChannel<ProdInfo>   prodNoticeChan;
    RegChannel<ChunkInfo>  chunkNoticeChan;
    RegChannel<ProdIndex>  prodReqChan;
    RegChannel<ChunkInfo>  chunkReqChan;
    ChunkChannel           chunkChan;
    Socket                 sock;
    ObjQueue<ProdInfo>     prodNoticeQ;
    ObjQueue<ChunkInfo>    chunkNoticeQ;
    ObjQueue<ProdIndex>    prodReqQ;
    ObjQueue<ChunkInfo>    chunkReqQ;
    ObjQueue<LatentChunk>  chunkQ;
    RecvQueue              recvQ;

    /**
     * Returns the protocol version of the remote peer.
     * @pre `sock.getStreamId() == VERSION_STREAM_ID`
     * @return Protocol version of the remote peer
     * @throws std::logic_error if precondition not met
     * @threadsafety Safe
     */
     unsigned getVersion();

public:
    /**
     * Constructs from an SCTP socket.
     * @param[in,out] sock     Socket
     * @throws std::logic_error if the protocol version of the remote peer can't
     *                          be handled
     */
    PeerImpl(
            Socket&  sock);
    /**
     * Returns the number of streams.
     */
    static unsigned getNumStreams();
    /**
     * Sends information about a product to the remote peer.
     * @param[in] prodInfo  Product information
     */
    void sendProdInfo(const ProdInfo& prodInfo);
    /**
     * Sends information about a chunk-of-data to the remote peer.
     * @param[in] chunkInfo  Chunk information
     */
    void sendChunkInfo(const ChunkInfo& chunkInfo);
    /**
     * Sends a request for product information to the remote peer.
     * @param[in] prodIndex  Product-index
     */
    void sendProdRequest(const ProdIndex& prodIndex);
    /**
     * Sends a request for a chunk-of-data to the remote peer.
     * @param[in] info  Chunk specification
     */
    void sendRequest(const ChunkInfo& info);
    /**
     * Sends a chunk-of-data to the remote peer.
     * @param[in] chunk  Chunk-of-data
     */
    void sendData(const ActualChunk& chunk);
    /**
     * Returns the type of the next message. Blocks until one arrives if
     * necessary.
     */
    MsgType getMsgType();
    /**
     * Returns a notice of a product.
     * @return Information on a product
     * @threadsafety Safe
     */
    ProdInfo getProdNotice();
    /**
     * Returns a notice of a chunk-of-data.
     * @pre `getMsgType() == MSGTYPE_CHUNK_NOTICE`
     * @return Chunk-of-data
     * @throws std::logic_error if precondition not met
     * @threadsafety Safe
     */
    ChunkInfo getChunkNotice();
    /**
     * Returns a request for information on a product.
     * @pre `getMsgType() == MSGTYPE_PROD_REQUEST`
     * @return Product index
     * @throws std::logic_error if precondition not met
     * @threadsafety Safe
     */
    ProdIndex getProdRequest();
    /**
     * Returns a request for a chunk-of-data.
     * @pre `getMsgType() == MSGTYPE_CHUNK_REQUEST`
     * @return Information on the chunk
     * @throws std::logic_error if precondition not met
     * @threadsafety Safe
     */
    ChunkInfo getChunkRequest();
    /**
     * Returns a latent chunk-of-data.
     * @pre `getMsgType() == MSGTYPE_CHUNK`
     * @return Latent chunk-of-data
     * @throws std::logic_error if precondition not met
     * @threadsafety Safe
     */
    LatentChunk getChunk();
};

} // namespace

#endif /* PEERIMPL_H_ */
