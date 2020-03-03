/**
 * The types involved in network exchanges.
 *
 * Copyright 2020 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: hycast.cpp
 *  Created on: Oct 28, 2019
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "hycast.h"

#include "error.h"
#include "Peer.h"
#include "Repository.h"

#include <cstring>

namespace hycast {

/******************************************************************************/

std::string ProdIndex::to_string() const
{
    return std::to_string(index);
}

/******************************************************************************/

Flags::Flags() noexcept
    : flags{0}
{}

Flags::Flags(const Type flags) noexcept
    : flags{flags}
{}

Flags::Flags(const Flags& flags) noexcept
    : Flags{flags.flags.load()}
{}

Flags& Flags::operator =(const Type flags) noexcept
{
    this->flags = flags;
    return *this;
}

Flags::operator Type() const noexcept
{
    return flags.load();
}

Flags& Flags::setPathToSrc() noexcept
{
    flags |= SRC_PATH;
    return *this;
}

Flags& Flags::clrPathToSrc() noexcept
{
    flags &= ~SRC_PATH;
    return *this;
}

bool Flags::isPathToSrc() const noexcept
{
    return flags & SRC_PATH;
}

Flags& Flags::setProd() noexcept
{
    flags |= PROD;
    return *this;
}

bool Flags::isProd() const noexcept
{
    return flags & PROD;
}

/******************************************************************************/

class ProdInfo::Impl
{
    const ProdIndex   prodIndex;
    const ProdSize    size;
    const std::string name;

public:
    Impl(   const ProdIndex    prodIndex,
            const ProdSize     size,
            const std::string& name)
        : prodIndex{prodIndex}
        , size{size}
        , name{name}
    {}

    ProdIndex getIndex() const
    {
        return prodIndex;
    }

    ProdSize getSize() const
    {
        return size;
    }

    const std::string& getName() const
    {
        return name;
    }

    std::string to_string() const
    {
        return "{name: " + name + ", index: " + prodIndex.to_string() +
                ", size: " + std::to_string(size) + "}";
    }

    bool operator ==(const Impl& rhs) const noexcept
    {
        return prodIndex == rhs.prodIndex &&
               size == rhs.size &&
               name == rhs.name;
    }

    void send(PeerProto& proto)
    {
        proto.send(ProdInfo(prodIndex, size, name));
    }
};

/******************************************************************************/

/******************************************************************************/

ProdInfo::ProdInfo()
{}

ProdInfo::ProdInfo(
        const ProdIndex    prodIndex,
        const ProdSize     size,
        const std::string& name)
    : pImpl(new Impl(prodIndex, size, name))
{}

ProdInfo::operator bool() const noexcept
{
    return static_cast<bool>(pImpl);
}

ProdIndex ProdInfo::getProdIndex() const
{
    return pImpl->getIndex();
}

ProdSize ProdInfo::getProdSize() const
{
    return pImpl->getSize();
}

const std::string& ProdInfo::getProdName() const
{
    return pImpl->getName();
}

std::string ProdInfo::to_string() const
{
    return pImpl->to_string();
}

bool ProdInfo::operator ==(const ProdInfo& rhs) const noexcept
{
    return *pImpl.get() == *rhs.pImpl.get();
}

void ProdInfo::send(PeerProto& proto) const
{
    proto.send(*this);
}

/******************************************************************************/

std::string SegId::to_string() const
{
    return "{prodIndex: " + prodIndex.to_string() + ", segOffset: " +
            std::to_string(segOffset) + "}";
}

/******************************************************************************/

std::string SegInfo::to_string() const
{
    return "{segId: " + id.to_string() + ", prodSize: " +
            std::to_string(prodSize) + ", segSize: " +
            std::to_string(segSize) + "}";
}

/******************************************************************************/

bool ChunkId::operator ==(const ChunkId& rhs) const
{
    return (isProd == rhs.isProd) &&
            (isProd
                ? id.prodIndex == rhs.id.prodIndex
                : id.segId == rhs.id.segId);

}

std::string ChunkId::to_string() const
{
    return isProd
            ? id.prodIndex.to_string()
            : id.segId.to_string();
}

void ChunkId::notify(PeerProto& peerProto) const
{
    try {
        if (isProd) {
            LOG_DEBUG("Notifying about product %s",
                    id.prodIndex.to_string().data());
            peerProto.notify(id.prodIndex);
        }
        else {
            LOG_DEBUG("Notifying about segment %s",
                    id.segId.to_string().c_str());
            peerProto.notify(id.segId);
        }
        //LOG_DEBUG("Notified");
    }
    catch (const std::exception& ex) {
        LOG_DEBUG("Caught exception \"%s\"", ex.what());
        std::throw_with_nested(RUNTIME_ERROR("Couldn't notify about chunk " +
                to_string()));
    }
    catch (...) {
        LOG_DEBUG("Caught exception ...");
        throw;
    }
}

void ChunkId::request(PeerProto& peerProto) const
{
    if (isProd) {
        LOG_DEBUG("Requesting information about product %s",
                id.prodIndex.to_string().data());
        peerProto.request(id.prodIndex);
    }
    else {
        LOG_DEBUG("Requesting segment %s",
                id.segId.to_string().c_str());
        peerProto.request(id.segId);
    }
}

void ChunkId::request(Peer& peer) const
{
    if (isProd) {
        LOG_DEBUG("Requesting information about product %s",
                id.prodIndex.to_string().data());
        peer.request(id.prodIndex);
    }
    else {
        LOG_DEBUG("Requesting segment %s",
                id.segId.to_string().c_str());
        peer.request(id.segId);
    }
}

/******************************************************************************/

DataSeg::~DataSeg() noexcept
{}

/******************************************************************************/

class MemSeg::Impl
{
    const SegInfo info;
    const void*   data;

public:
    Impl(   const SegInfo& info,
            const void*    data)
        : info{info}
        , data{data}
    {}

    const SegInfo& getInfo() const noexcept
    {
        return info;
    }

    const SegId& getSegId() const noexcept
    {
        return info.getId();
    }

    const void* getData() const
    {
        return data;
    }

    SegSize getSegSize() const
    {
        return info.getSegSize();
    }

    ProdIndex getProdId() const noexcept
    {
        return info.getId().getProdIndex();
    }

    ProdSize getProdSize() const
    {
        return info.getProdSize();
    }

    ProdSize getOffset() const
    {
        return info.getId().getOffset();
    }

    std::string to_string() const
    {
        return info.to_string();
    }

    void copyData(void* buf)
    {
        ::memcpy(buf, data, getSegSize());
    }

    bool operator ==(const Impl& rhs) const
    {
        return info == rhs.info &&
                ::memcmp(data, rhs.data, info.getSegSize()) == 0;
    }
};

MemSeg::MemSeg()
{}

MemSeg::MemSeg(
        const SegInfo& info,
        const void*    data)
    : pImpl(new Impl(info, data))
{}

MemSeg::operator bool() const noexcept
{
    return static_cast<bool>(pImpl);
}

const SegInfo& MemSeg::getSegInfo() const noexcept
{
    return pImpl->getInfo();
}

const SegId& MemSeg::getSegId() const noexcept
{
    return pImpl->getSegId();
}

const void* MemSeg::data() const
{
    return pImpl->getData();
}

SegSize MemSeg::getSegSize() const
{
    return pImpl->getSegSize();
}

ProdIndex MemSeg::getProdIndex() const noexcept
{
    return pImpl->getProdId();
}

ProdSize MemSeg::getProdSize() const
{
    return pImpl->getProdSize();
}

ProdSize MemSeg::getOffset() const noexcept
{
    return pImpl->getOffset();
}

std::string MemSeg::to_string() const
{
    return pImpl->to_string();
}

void MemSeg::getData(void* buf) const
{
    pImpl->copyData(buf);
}

bool MemSeg::operator ==(const MemSeg& rhs) const noexcept
{
    return *pImpl.get() == *rhs.pImpl.get();
}

/******************************************************************************/

class SockSeg::Impl
{
protected:
    const SegInfo info;

    Impl(const SegInfo& info)
        : info{info}
    {}

public:
    virtual ~Impl()
    {}

    const SegInfo& getSegInfo() const noexcept
    {
        return info;
    }

    const SegId& getSegId() const noexcept
    {
        return info.getId();
    }

    ProdIndex getProdId() const noexcept
    {
        return info.getId().getProdIndex();
    }

    virtual std::string to_string() const =0;
};

SockSeg::SockSeg(Impl* impl)
    : pImpl{impl}
{}

const SegInfo& SockSeg::getSegInfo() const noexcept
{
    return pImpl->getSegInfo();
}

const SegId& SockSeg::getSegId() const noexcept
{
    return pImpl->getSegId();
}

ProdIndex SockSeg::getProdIndex() const noexcept
{
    return pImpl->getProdId();
}

/******************************************************************************/

class UdpSeg::Impl final : public SockSeg::Impl
{
    UdpSock sock;

public:
    Impl(   const SegInfo& info,
            UdpSock&       sock)
        : SockSeg::Impl{info}
        , sock{sock}
    {}

    std::string to_string() const
    {
        return "{segInfo: " + info.to_string() + ", udpSock: " +
                sock.to_string() + "}";
    }

    void read(void* buf)
    {
        const auto nbytes = getSegInfo().getSegSize();
        sock.addPeek(buf, nbytes);
        sock.peek();
    }

    ProdSize getOffset() const noexcept
    {
        return info.getId().getOffset();
    }
};

UdpSeg::UdpSeg(
        const SegInfo& info,
        UdpSock&       sock)
    : SockSeg{new Impl(info, sock)}
{}

std::string UdpSeg::to_string() const
{
    return static_cast<Impl*>(SockSeg::pImpl.get())->to_string();
}

void UdpSeg::read(void* buf) const
{
    return static_cast<Impl*>(SockSeg::pImpl.get())->read(buf);
}

ProdSize UdpSeg::getOffset() const noexcept
{
    return static_cast<Impl*>(SockSeg::pImpl.get())->getOffset();
}

/******************************************************************************/

class TcpSeg::Impl final : public SockSeg::Impl
{
    TcpSock sock;

public:
    Impl(   const SegInfo& info,
            TcpSock&       sock)
        : SockSeg::Impl{info}
        , sock{sock}
    {}

    std::string to_string() const
    {
        return "{segInfo: " + info.to_string() + ", tcpSock: " +
                sock.to_string() + "}";
    }

    void read(void* buf)
    {
        const auto nbytes = getSegInfo().getSegSize();
        if (!sock.read(buf, nbytes))
            throw EOF_ERROR("EOF");
    }

    ProdSize getOffset() const noexcept
    {
        return info.getId().getOffset();
    }
};

TcpSeg::TcpSeg(
        const SegInfo& info,
        TcpSock&       sock)
    : SockSeg{new Impl(info, sock)}
{}

std::string TcpSeg::to_string() const
{
    return static_cast<Impl*>(SockSeg::pImpl.get())->to_string();
}

void TcpSeg::read(void* buf) const
{
    return static_cast<Impl*>(SockSeg::pImpl.get())->read(buf);
}

ProdSize TcpSeg::getOffset() const noexcept
{
    return static_cast<Impl*>(SockSeg::pImpl.get())->getOffset();
}

SockSeg::~SockSeg()
{}

} // namespace

namespace std {
    string to_string(const hycast::ProdInfo& prodInfo)
    {
        return prodInfo.to_string();
    }
}
