/**
 * This file implements a connection between Hycast peers.
 *
 * Copyright 2016 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: PeerConnectionImpl.cpp
 * @author: Steven R. Emmerson
 */

#include <iostream>
#include <pthread.h>
#include <PeerImpl.h>

namespace hycast {

PeerImpl::PeerImpl(
        PeerMgr&       peerMgr,
        Socket&        sock)
    : version(0),
      versionChan(sock, VERSION_STREAM_ID, version),
      prodNoticeChan(sock, PROD_NOTICE_STREAM_ID, version),
      chunkNoticeChan(sock, CHUNK_NOTICE_STREAM_ID, version),
      prodReqChan(sock, PROD_REQ_STREAM_ID, version),
      chunkReqChan(sock, CHUNK_REQ_STREAM_ID, version),
      chunkChan(sock, CHUNK_STREAM_ID, version),
      peerMgr(&peerMgr),
      sock(sock),
      recvThread(std::thread(&PeerImpl::runReceiver, std::ref(*this)))
{
}

PeerImpl::~PeerImpl()
{
    try {
        (void)pthread_cancel(recvThread.native_handle());
        recvThread.join();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    catch (...) {
        // Never throw an exception in a destructor
    }
}

void PeerImpl::sendProdInfo(const ProdInfo& prodInfo)
{
    prodNoticeChan.send(prodInfo);
}

void PeerImpl::sendChunkInfo(const ChunkInfo& chunkInfo)
{
    chunkNoticeChan.send(chunkInfo);
}

void PeerImpl::sendProdRequest(const ProdIndex& prodIndex)
{
    prodReqChan.send(prodIndex);
}

void PeerImpl::sendRequest(const ChunkInfo& info)
{
    chunkReqChan.send(info);
}

void PeerImpl::sendData(const ActualChunk& chunk)
{
    chunkChan.send(chunk);
}

void hycast::PeerImpl::runReceiver()
{
    try {
        for (;;) {
            int cancelState;
            (void)pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &cancelState);
            uint32_t size = sock.getSize();
            (void)pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancelState);
            if (size == 0) {
                peerMgr->recvEof();
                break; // remote peer closed connection
            }
            switch (sock.getStreamId()) {
                case PROD_NOTICE_STREAM_ID:
                    peerMgr->recvNotice(prodNoticeChan.recv());
                    break;
                case CHUNK_NOTICE_STREAM_ID:
                    peerMgr->recvNotice(chunkNoticeChan.recv());
                    break;
                case PROD_REQ_STREAM_ID:
                    peerMgr->recvRequest(prodReqChan.recv());
                    break;
                case CHUNK_REQ_STREAM_ID:
                    peerMgr->recvRequest(chunkReqChan.recv());
                    break;
                case CHUNK_STREAM_ID: {
                    /*
                     * For an unknown reason, the compiler complains if the
                     * `peer->recvData` parameter is a `LatentChunk&` and not a
                     * `LatentChunk`.  This is acceptable, however, because
                     * `LatentChunk` uses the pImpl idiom. See
                     * `PeerMgr::recvData`.
                     */
                    peerMgr->recvData(chunkChan.recv());
                    if (sock.hasMessage())
                        throw std::logic_error("Data not drained from latent "
                                "chunk-of-data");
                    break;
                }
                default:
                    sock.discard();
            }
        }
    }
    catch (const std::exception& e) {
        peerMgr->recvException(e);
    }
}

unsigned PeerImpl::getNumStreams()
{
    return NUM_STREAM_IDS;
}

} // namespace
