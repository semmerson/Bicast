/**
 * For sending and receiving multicast chunks.
 *
 *        File: Multicast.cpp
 *  Created on: Oct 15, 2019
 *      Author: Steven R. Emmerson
 *
 *    Copyright 2021 University Corporation for Atmospheric Research
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <main/inet/Socket.h>
#include "config.h"

#include "error.h"
#include "ProdInfo.h"
#include "UdpXprt.h"

#include <sys/uio.h>

namespace hycast {

static const Flags FLAGS_INFO = 1;

class UdpXprt::Impl {
    UdpSock sock;

    UdpInfoChunk recvInfo(
            const ProdIndex prodIndex,
            const ProdSize  prodSize,
            const SegSize   nameLen)
    {
        struct iovec iov;

        std::string name(nameLen, '\0');
        iov.iov_base = name.c_str();
        iov.iov_len = nameLen;

        sock.read(&iov, 1);
        sock.discard();

        return UdpInfoChunk(prodIndex, ProdInfo(name, prodSize));
    }

    UdpSegChunk recvData(
            const ProdIndex prodIndex,
            const ProdSize  prodSize,
            const SegSize   dataLen)
    {
        struct iovec iov;

        ProdSize segOffset;
        iov.iov_base = &segOffset;
        iov.iov_len = sizeof(segOffset);

        sock.read(&iov, 1);

        segOffset = sock.ntoh(segOffset);

        return UdpSegChunk(prodIndex, prodSize, segOffset, sock);
    }

public:
    Impl(const SockAddr& grpAddr)
        : sock(grpAddr)
    {}

    Impl(   const SockAddr& grpAddr,
            const InetAddr& srcAddr)
        : sock(grpAddr, srcAddr)
    {}

    SockAddr getLclAddr() const
    {
        return sock.getLclAddr();
    }

    void send(const MemInfoChunk& chunk)
    {
        struct iovec iov[5];

        Flags     flags = FLAGS_INFO;
        flags = sock.hton(flags);
        iov[0].iov_base = &flags;
        iov[0].iov_len = sizeof(flags);

        const ProdInfo&    prodInfo = chunk.getProdInfo();
        const std::string& name = prodInfo.getProdName();
        SegSize            nameLen = name.length();

        iov[4].iov_base = name.c_str();
        iov[4].iov_len = nameLen;

        nameLen = sock.hton(nameLen);
        iov[1].iov_base = &nameLen;
        iov[1].iov_len = sizeof(nameLen);

        ProdIndex prodIndex = chunk.getSegId().getProdIndex();
        prodIndex = sock.hton(prodIndex);
        iov[2].iov_base = &prodIndex;
        iov[2].iov_len = sizeof(prodIndex);

        ProdSize prodSize = prodInfo.getProdSize();
        prodSize = sock.hton(prodSize);
        iov[3].iov_base = &prodSize;
        iov[3].iov_len = sizeof(prodSize);

        sock.write(iov, 5);
    }

    void send(const MemSeg& chunk)
    {
        struct iovec iov[6];

        static const Flags flags = 0; // Is product-data
        iov[0].iov_base = &flags;
        iov[0].iov_len = sizeof(flags);

        const ProdInfo&  prodInfo = chunk.getProdSize();
        SegSize          segSize = chunk.getSegSize();

        iov[5].iov_base = chunk.data();
        iov[5].iov_len = segSize;

        segSize = sock.hton(segSize);
        iov[1].iov_base = &segSize;
        iov[1].iov_len = sizeof(segSize);

        ProdIndex prodIndex = chunk.getSegId().getProdIndex();
        prodIndex = sock.hton(prodIndex);
        iov[2].iov_base = &prodIndex;
        iov[2].iov_len = sizeof(prodIndex);

        ProdSize prodSize = prodInfo.getProdSize();
        prodSize = sock.hton(prodSize);
        iov[3].iov_base = &prodSize;
        iov[3].iov_len = sizeof(prodSize);

        ProdSize segOffset = chunk.getSegOffset();
        segOffset = sock.hton(segOffset);
        iov[4].iov_base = &segOffset;
        iov[4].iov_len = sizeof(segOffset);

        sock.write(iov, 6);
    }

    UdpChunk recv()
    {
        struct iovec iov[4];

        Flags flags;
        iov[0].iov_base = &flags;
        iov[0].iov_len = sizeof(flags);

        SegSize varSize;
        iov[1].iov_base = &varSize;
        iov[1].iov_len = sizeof(varSize);

        ProdIndex prodIndex;
        iov[2].iov_base = &prodIndex;
        iov[2].iov_len = sizeof(prodIndex);

        ProdSize prodSize;
        iov[3].iov_base = &prodSize;
        iov[3].iov_len = sizeof(prodSize);

        sock.read(iov, 4);

        flags = sock.ntoh(flags);
        varSize = sock.ntoh(varSize);
        prodIndex = sock.ntoh(prodIndex);
        prodSize = sock.ntoh(prodSize);

        return (flags & FLAGS_INFO)
            ? recvInfo(prodIndex, prodSize, varSize)
            : recvData(prodIndex, prodSize, varSize);
    }
};

UdpXprt::UdpXprt(const SockAddr& grpAddr)
    : pImpl{new Impl(grpAddr)}
{}

UdpXprt::UdpXprt(
        const SockAddr& grpAddr,
        const InetAddr& srcAddr)
    : pImpl{new Impl(grpAddr, srcAddr)}
{}

void UdpXprt::send(const MemInfoChunk& chunk) const
{
    pImpl->send(chunk);
}

void UdpXprt::send(const MemSeg& chunk) const
{
    pImpl->send(chunk);
}

UdpChunk UdpXprt::recv()
{
    return pImpl->recv();
}

} // namespace
