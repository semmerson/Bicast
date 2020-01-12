/**
 * Multicast protocol.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: McastProto.cpp
 *  Created on: Nov 5, 2019
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "error.h"
#include "McastProto.h"

namespace hycast {

class McastSndr::Impl {
    UdpSock sock;

public:
    Impl(UdpSock& sock)
        : sock{sock}
    {}

    Impl(UdpSock&& sock)
        : sock{sock}
    {}

    void setMcastIface(const InetAddr& interface)
    {
        sock.setMcastIface(interface);
    }

    void multicast(const ProdInfo& prodInfo)
    {
        struct iovec iov[5];

        // Product information
        static Flags::flags_t flags = sock.hton(Flags::getProd());
        iov[0].iov_base = &flags;
        iov[0].iov_len = sizeof(flags);

        // Done now before 'hton()` conversion
        const std::string& name = prodInfo.getName();
        SegSize            nameLen = name.length();
        iov[4].iov_base = const_cast<char*>(name.data()); // Safe casts
        iov[4].iov_len = nameLen;

        nameLen = sock.hton(nameLen);
        iov[1].iov_base = &nameLen;
        iov[1].iov_len = sizeof(nameLen);

        ProdIndex prodId = prodInfo.getProdIndex();
        auto   value = sock.hton(prodId.getValue());
        iov[2].iov_base = &value;
        iov[2].iov_len = sizeof(value);

        ProdSize prodSize = prodInfo.getSize();
        prodSize = sock.hton(prodSize);
        iov[3].iov_base = &prodSize;
        iov[3].iov_len = sizeof(prodSize);

        sock.write(iov, 5);
    }

    void multicast(const MemSeg& seg)
    {
        struct iovec iov[6];

        // Data-segment
        static Flags::flags_t flags = sock.hton(Flags::getSeg());
        iov[0].iov_base = &flags;
        iov[0].iov_len = sizeof(flags);

        SegSize segSize = seg.getSegSize();

        // Done now before 'hton()` conversion
        iov[5].iov_base = const_cast<void*>(seg.getData());
        iov[5].iov_len = segSize;

        segSize = sock.hton(segSize);
        iov[1].iov_base = &segSize;
        iov[1].iov_len = sizeof(segSize);

        ProdIndex prodId = seg.getProdIndex();
        auto   value = sock.hton(prodId.getValue());
        iov[2].iov_base = &value;
        iov[2].iov_len = sizeof(value);

        ProdSize prodSize = seg.getProdSize();
        prodSize = sock.hton(prodSize);
        iov[3].iov_base = &prodSize;
        iov[3].iov_len = sizeof(prodSize);

        ProdSize segOffset = seg.getSegOffset();
        segOffset = sock.hton(segOffset);
        iov[4].iov_base = &segOffset;
        iov[4].iov_len = sizeof(segOffset);

        sock.write(iov, 6);
    }
};

McastSndr::McastSndr(UdpSock& sock)
    : pImpl{new Impl(sock)}
{}

McastSndr::McastSndr(UdpSock&& sock)
    : pImpl{new Impl(sock)}
{}

const McastSndr& McastSndr::setMcastIface(const InetAddr& iface) const
{
    pImpl->setMcastIface(iface);
    return *this;
}

void McastSndr::multicast(const ProdInfo& info)
{
    pImpl->multicast(info);
}

void McastSndr::multicast(const MemSeg& seg)
{
    pImpl->multicast(seg);
}

/******************************************************************************/

class McastRcvr::Impl
{
    UdpSock       sock;
    McastRcvrObs* srvr;

    void recvInfo(
            const ProdIndex prodIndex,
            const ProdSize  prodSize,
            const SegSize   nameLen)
    {
        struct iovec iov;

        char buf[nameLen];
        iov.iov_base = buf;
        iov.iov_len = nameLen;

        auto nread = sock.read(&iov, 1);
        if (nread != nameLen)
            throw RUNTIME_ERROR("Couldn't read product name");

        std::string name(buf, nameLen);
        ProdInfo    prodInfo{prodIndex, prodSize, name};
        srvr->hereIs(prodInfo);
    }

    void recvSeg(
            const ProdIndex prodIndex,
            const ProdSize  prodSize,
            const SegSize   segSize)
    {
        struct iovec iov;

        ProdSize segOffset;
        iov.iov_base = &segOffset;
        iov.iov_len = sizeof(segOffset);

        auto nread = sock.read(&iov, 1);
        if (nread != sizeof(segOffset))
            throw RUNTIME_ERROR("Couldn't read segment offset");

        segOffset = sock.ntoh(segOffset);

        SegId   id(prodIndex, segOffset);
        SegInfo info(id, prodSize, segSize);
        UdpSeg  seg{info, sock};
        srvr->hereIs(seg);
    }

public:
    Impl(   UdpSock&   sock,
            McastRcvrObs& srvr)
        : sock{sock}
        , srvr{&srvr}
    {}

    /**
     * Returns on EOF.
     */
    void operator()()
    {
        for (;;) {
            struct iovec iov[4];

            Flags::flags_t flags;
            iov[0].iov_base = &flags;
            iov[0].iov_len = sizeof(flags);

            SegSize varSize;
            iov[1].iov_base = &varSize;
            iov[1].iov_len = sizeof(varSize);

            ProdIndex::ValueType id;
            iov[2].iov_base = &id;
            iov[2].iov_len = sizeof(id);

            ProdSize prodSize;
            iov[3].iov_base = &prodSize;
            iov[3].iov_len = sizeof(prodSize);

            auto nread = sock.read(iov, 4);
            if (nread == 0)
                break; // EOF
            if (nread != sizeof(flags) + sizeof(varSize) + sizeof(id) +
                    sizeof(prodSize))
                throw RUNTIME_ERROR("Couldn't read packet header");

            flags = sock.ntoh(flags);
            varSize = sock.ntoh(varSize);
            ProdIndex prodId{sock.ntoh(id)};
            prodSize = sock.ntoh(prodSize);

            (Flags::isProd(flags))
                ? recvInfo(prodId, prodSize, varSize)
                : recvSeg(prodId, prodSize, varSize);

            sock.discard();
        } // Indefinite loop
    }

    /**
     * Causes `operator()()` to return.
     *
     * @throws    SystemError      `::shutdown()` failure
     */
    void halt()
    {
        sock.shutdown(SHUT_RD);
    }
};

McastRcvr::McastRcvr(
        UdpSock&      sock,
        McastRcvrObs& srvr)
    : pImpl{new Impl(sock, srvr)}
{}

void McastRcvr::operator()()
{
    pImpl->operator()();
}

void McastRcvr::halt()
{
    pImpl->halt();
}

} // namespace
