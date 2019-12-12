/**
 * Dispatcher of incoming messages to appropriate methods.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Dispatcher.cpp
 *  Created on: Nov 4, 2019
 *      Author: Steven R. Emmerson
 */

#include <PeerProto.h>
#include "config.h"


namespace hycast {

static const Flags FLAGS_INFO = 1;

class InProto::Impl {
public:
    virtual ~Impl()
    {}

    virtual void dispatch(UdpSock& sock) const =0;

    virtual void dispatch(TcpSock& sock) const =0;
};

/******************************************************************************/

class ChunkDispatcher::Impl final : public InProto::Impl
{
private:
    ChunkSrvr srvr;

    void dispatchInfo(
            const ProdIndex prodIndex,
            const ProdSize  prodSize,
            const SegSize   nameLen,
            UdpSock&        sock)
    {
        struct iovec iov;

        std::string name(nameLen, '\0');
        iov.iov_base = name.c_str();
        iov.iov_len = nameLen;

        sock.read(name.c_str(), nameLen);
        sock.discard();

        InfoChunk chunk{prodIndex, ProdInfo{name, prodSize}};
        srvr.handle(chunk);
    }

    void dispatchData(
            const ProdIndex prodIndex,
            const ProdSize  prodSize,
            const SegSize   dataLen,
            UdpSock&        sock)
    {
        struct iovec iov;

        ProdSize segOffset;
        iov.iov_base = &segOffset;
        iov.iov_len = sizeof(segOffset);

        sock.read(&iov, 1);

        segOffset = sock.ntoh(segOffset);

        UdpDataChunk chunk{prodIndex, prodSize, segOffset, sock};
        srvr.handle(chunk);
    }

    void dispatchInfo(
            const ProdIndex prodIndex,
            const ProdSize  prodSize,
            const SegSize   nameLen,
            TcpSock&        sock)
    {
        std::string name(nameLen, '\0');

        sock.read(static_cast<void*>(name.data()), nameLen);

        InfoChunk chunk{prodIndex, ProdInfo{name, prodSize}};
        srvr.handle(chunk);
    }

    void dispatchData(
            const ProdIndex prodIndex,
            const ProdSize  prodSize,
            const SegSize   dataLen,
            TcpSock&        sock)
    {
        ProdSize segOffset;

        sock.read(segOffset); // Performs network-to-host translation

        TcpDataChunk chunk{prodIndex, prodSize, segOffset, sock};
        srvr.handle(chunk);
    }

public:
    Impl(ChunkSrvr& srvr)
        : InProto::Impl{}
        , srvr{srvr}
    {}

    void dispatch(UdpSock& sock) const override
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

        (flags & FLAGS_INFO)
            ? dispatchInfo(prodIndex, prodSize, varSize, sock)
            : dispatchData(prodIndex, prodSize, varSize, sock);
    }

    void dispatch(TcpSock& sock) const override
    {
        Flags flags;
        sock.read(flags);

        SegSize varSize;
        sock.read(varSize);

        ProdIndex prodIndex;
        sock.read(prodIndex);

        ProdSize prodSize;
        sock.read(prodSize);

        (flags & FLAGS_INFO)
            ? dispatchInfo(prodIndex, prodSize, varSize, sock)
            : dispatchData(prodIndex, prodSize, varSize, sock);
    }
};

} // namespace
