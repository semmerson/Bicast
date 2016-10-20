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

#include "PeerConnectionImpl.h"

#include <iostream>
#include <pthread.h>

namespace hycast {

PeerConnectionImpl::PeerConnectionImpl(
        Peer&          peer,
        Socket&        sock,
        const unsigned version)
    : prodNoticeChan(sock, PROD_NOTICE_STREAM_ID, version),
      chunkNoticeChan(sock, CHUNK_NOTICE_STREAM_ID, version),
      prodReqChan(sock, PROD_REQ_STREAM_ID, version),
      chunkReqChan(sock, CHUNK_REQ_STREAM_ID, version),
      chunkChan(sock, CHUNK_STREAM_ID, version),
      peer(&peer),
      sock(sock),
      version(version),
      recvThread(std::thread(&PeerConnectionImpl::runReceiver, std::ref(*this)))
{
}

PeerConnectionImpl::~PeerConnectionImpl()
{
    try {
        (void)pthread_cancel(recvThread.native_handle());
        recvThread.join();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    catch (...) {
    }
}

void PeerConnectionImpl::sendProdInfo(const ProdInfo& prodInfo)
{
    prodNoticeChan.send(prodInfo);
}

void PeerConnectionImpl::sendChunkInfo(const ChunkInfo& chunkInfo)
{
    chunkNoticeChan.send(chunkInfo);
}

void PeerConnectionImpl::sendProdRequest(const ProdIndex& prodIndex)
{
    prodReqChan.send(prodIndex);
}

void PeerConnectionImpl::sendRequest(const ChunkInfo& info)
{
    chunkReqChan.send(info);
}

void PeerConnectionImpl::sendData(const ActualChunk& chunk)
{
    chunkChan.send(chunk);
}

void hycast::PeerConnectionImpl::runReceiver()
{
    try {
        for (;;) {
            int cancelState;
            (void)pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &cancelState);
            uint32_t size = sock.getSize();
            (void)pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cancelState);
            if (size == 0) {
                peer->recvEof();
                break; // remote peer closed connection
            }
            switch (sock.getStreamId()) {
                case PROD_NOTICE_STREAM_ID:
                    peer->recvNotice(prodNoticeChan.recv());
                    break;
                case CHUNK_NOTICE_STREAM_ID:
                    peer->recvNotice(chunkNoticeChan.recv());
                    break;
                case PROD_REQ_STREAM_ID:
                    peer->recvRequest(prodReqChan.recv());
                    break;
                case CHUNK_REQ_STREAM_ID:
                    peer->recvRequest(chunkReqChan.recv());
                    break;
                case CHUNK_STREAM_ID:
                    peer->recvData(chunkChan.recv());
                    break;
                default:
                    sock.discard();
            }
        }
    }
    catch (const std::exception& e) {
        peer->recvException(e);
    }
}

}
 // namespace
