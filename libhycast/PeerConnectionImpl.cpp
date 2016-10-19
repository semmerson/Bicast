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
    : prodInfoChan(sock, PROD_INFO_STREAM_ID, version),
      chunkInfoChan(sock, CHUNK_INFO_STREAM_ID, version),
      prodIndexChan(sock, PROD_INFO_REQ_STREAM_ID, version),
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
    prodInfoChan.send(prodInfo);
}

void PeerConnectionImpl::sendChunkInfo(const ChunkInfo& chunkInfo)
{
    chunkInfoChan.send(chunkInfo);
}

void PeerConnectionImpl::sendProdRequest(const ProdIndex& prodIndex)
{
    prodIndexChan.send(prodIndex);
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
                case PROD_INFO_STREAM_ID:
                    peer->recvNotice(prodInfoChan.recv());
                    break;
                case CHUNK_INFO_STREAM_ID:
                    peer->recvNotice(chunkInfoChan.recv());
                    break;
                case PROD_INFO_REQ_STREAM_ID:
                    peer->recvRequest(prodIndexChan.recv());
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
