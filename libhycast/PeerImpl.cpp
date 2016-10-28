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
#include <stdexcept>

namespace hycast {

template<class T>
PeerImpl::ObjQueue<T>::ObjQueue()
    : mutex{},
      cond{},
      haveObj{false},
      obj{}
{}

template<class T>
void PeerImpl::ObjQueue<T>::put(T& obj)
{
    std::unique_lock<std::mutex> lock{mutex};
    while (haveObj)
        cond.wait(lock);
    this->obj = obj;
    haveObj = true;
    cond.notify_one();
}

template<class T>
T PeerImpl::ObjQueue<T>::get()
{
    std::unique_lock<std::mutex> lock{mutex};
    while (!haveObj)
        cond.wait(lock);
    haveObj = false;
    cond.notify_one();
    return obj;
}

PeerImpl::RecvQueue::RecvQueue()
    : mutex{},
      cond{},
      circBuf{},
      nextPut{0},
      nextGet{0}
{}

void PeerImpl::RecvQueue::put(MsgType type)
{
    std::unique_lock<std::mutex> lock{mutex};
    while ((nextPut + 1)%nelt != nextGet)
        cond.wait(lock);
    circBuf[nextPut++] = type;
    nextPut %= nelt;
    cond.notify_one();
}

MsgType PeerImpl::RecvQueue::get()
{
    std::unique_lock<std::mutex> lock{mutex};
    while (nextGet == nextPut)
        cond.wait(lock);
    MsgType type = circBuf[nextGet++];
    nextGet %= nelt;
    cond.notify_one();
    return type;
}

bool PeerImpl::RecvQueue::empty() const
{
    std::unique_lock<std::mutex> lock{mutex};
    return nextGet == nextPut;
}

PeerImpl::PeerImpl(
        Socket&        sock)
    : version(0),
      versionChan(sock, VERSION_STREAM_ID, version),
      prodNoticeChan(sock, PROD_NOTICE_STREAM_ID, version),
      chunkNoticeChan(sock, CHUNK_NOTICE_STREAM_ID, version),
      prodReqChan(sock, PROD_REQ_STREAM_ID, version),
      chunkReqChan(sock, CHUNK_REQ_STREAM_ID, version),
      chunkChan(sock, CHUNK_STREAM_ID, version),
      sock(sock),
      prodNoticeQ{},
      chunkNoticeQ{},
      prodReqQ{},
      chunkReqQ{},
      chunkQ{},
      recvQ{}
{
    versionChan.send(VersionMsg(version));
    const unsigned vers = getVersion();
    if (vers != version)
        throw std::logic_error("Unknown protocol version: " +
                std::to_string(vers));
}

unsigned PeerImpl::getNumStreams()
{
    return NUM_STREAM_IDS;
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

MsgType PeerImpl::getMsgType()
{
    if (recvQ.empty()) {
        MsgType type;
        for (;;) {
            if (sock.getSize() == 0) {
                return MSGTYPE_EOF;
            }
            else switch (sock.getStreamId()) {
                case PROD_NOTICE_STREAM_ID:
                    type = MSGTYPE_PROD_NOTICE;
                    break;
                case CHUNK_NOTICE_STREAM_ID:
                    type = MSGTYPE_CHUNK_NOTICE;
                    break;
                case PROD_REQ_STREAM_ID:
                    type = MSGTYPE_PROD_REQUEST;
                    break;
                case CHUNK_REQ_STREAM_ID:
                    type = MSGTYPE_CHUNK_REQUEST;
                    break;
                case CHUNK_STREAM_ID:
                    type = MSGTYPE_CHUNK;
                    break;
                default:
                    sock.discard();
                    continue;
            }
            break;
        }
        recvQ.put(type);
    }
    return recvQ.get();
}

unsigned PeerImpl::getVersion()
{
    if (sock.getStreamId() != VERSION_STREAM_ID)
        throw std::logic_error("Current message isn't a version message");
    return versionChan.recv();
}

ProdInfo PeerImpl::getProdNotice()
{
    return prodNoticeQ.get();
}

ChunkInfo PeerImpl::getChunkNotice()
{
    return chunkNoticeQ.get();
}

ProdIndex PeerImpl::getProdRequest()
{
    return prodReqQ.get();
}

ChunkInfo PeerImpl::getChunkRequest()
{
    return chunkReqQ.get();
}

LatentChunk PeerImpl::getChunk()
{
    return chunkQ.get();
}

} // namespace
