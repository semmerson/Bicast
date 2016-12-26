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

#include "PeerImpl.h"

#include <iostream>
#include <pthread.h>
#include <stdexcept>

namespace hycast {

PeerImpl::PeerImpl(Peer* peer)
    : version(0),
      versionChan(),
      prodNoticeChan(),
      chunkNoticeChan(),
      prodReqChan(),
      chunkReqChan(),
      chunkChan(),
      msgRcvr(defaultMsgRcvr),
      sock(),
      peer{peer}
{}

PeerImpl::PeerImpl(
        Peer*          peer,
        MsgRcvr&       msgRcvr,
        Socket&        sock)
    : version(0),
      versionChan(sock, VERSION_STREAM_ID, version),
      prodNoticeChan(sock, PROD_NOTICE_STREAM_ID, version),
      chunkNoticeChan(sock, CHUNK_NOTICE_STREAM_ID, version),
      prodReqChan(sock, PROD_REQ_STREAM_ID, version),
      chunkReqChan(sock, CHUNK_REQ_STREAM_ID, version),
      chunkChan(sock, CHUNK_STREAM_ID, version),
      msgRcvr(msgRcvr),
      sock(sock),
      peer(peer)
{
    versionChan.send(VersionMsg(version));
    const unsigned vers = getVersion();
    if (vers != version)
        throw std::logic_error("Unknown protocol version: " +
                std::to_string(vers));
}

unsigned PeerImpl::getVersion()
{
    if (sock.getStreamId() != VERSION_STREAM_ID)
        throw std::logic_error("Current message isn't a version message");
    return versionChan.recv();
}

unsigned PeerImpl::getNumStreams()
{
    return NUM_STREAM_IDS;
}

void PeerImpl::runReceiver()
{
    int entryCancelState;
    (void)pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &entryCancelState);
    for (;;) {
        (void)pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, nullptr);
        uint32_t size = sock.getSize();
        (void)pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, nullptr);
        if (size == 0)
            break;
        switch (sock.getStreamId()) {
            case PROD_NOTICE_STREAM_ID:
                msgRcvr.recvNotice(prodNoticeChan.recv(), *peer);
                break;
            case CHUNK_NOTICE_STREAM_ID:
                msgRcvr.recvNotice(chunkNoticeChan.recv(), *peer);
                break;
            case PROD_REQ_STREAM_ID:
                msgRcvr.recvRequest(prodReqChan.recv(), *peer);
                break;
            case CHUNK_REQ_STREAM_ID:
                msgRcvr.recvRequest(chunkReqChan.recv(), *peer);
                break;
            case CHUNK_STREAM_ID: {
                /*
                 * For an unknown reason, the compiler complains if the
                 * `peer->recvData` parameter is a `LatentChunk&` and not a
                 * `LatentChunk`.  This is acceptable, however, because
                 * `LatentChunk` can be trivially copied. See
                 * `PeerMgr::recvData()`.
                 */
                LatentChunk chunk = chunkChan.recv();
                msgRcvr.recvData(chunk, *peer);
                if (chunk.hasData())
                    throw std::logic_error(
                            "Latent chunk-of-data still has data");
                break;
            }
            default:
                sock.discard();
        }
    }
    (void)pthread_setcancelstate(entryCancelState, nullptr);
}

void PeerImpl::sendProdInfo(const ProdInfo& prodInfo) const
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

std::string PeerImpl::to_string() const
{
    return std::string("PeerImpl{sock=") + sock.to_string() + ", version=" +
            std::to_string(version) + "}";
}

} // namespace
