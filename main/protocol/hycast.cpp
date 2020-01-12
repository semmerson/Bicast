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
#include "PeerProto.h"
#include "Repository.h"

#include <cstring>

namespace hycast {

/******************************************************************************/

std::string ProdIndex::to_string() const
{
    return std::to_string(index);
}

/******************************************************************************/

class OutChunk::Impl
{};

OutChunk::OutChunk(Impl* impl)
    : pImpl{impl}
{}

void OutChunk::send(PeerProto& proto) const
{}

/******************************************************************************/

class InChunk::Impl
{};

InChunk::InChunk()
    : pImpl{}
{}

InChunk::InChunk(Impl* impl)
    : pImpl{impl}
{}

/******************************************************************************/

class ProdInfo::Impl final : public OutChunk::Impl, public InChunk::Impl
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
};

/******************************************************************************/

/******************************************************************************/

ProdInfo::ProdInfo()
    : OutChunk(nullptr)
{}

ProdInfo::ProdInfo(
        const ProdIndex    prodIndex,
        const ProdSize     size,
        const std::string& name)
    : OutChunk(new Impl(prodIndex, size, name))
{}

ProdInfo::operator bool() const noexcept
{
    return static_cast<bool>(OutChunk::pImpl);
}

ProdIndex ProdInfo::getProdIndex() const
{
    return static_cast<Impl*>(OutChunk::pImpl.get())->getIndex();
}

ProdSize ProdInfo::getSize() const
{
    return static_cast<Impl*>(OutChunk::pImpl.get())->getSize();
}

const std::string& ProdInfo::getName() const
{
    return static_cast<Impl*>(OutChunk::pImpl.get())->getName();
}

std::string ProdInfo::to_string() const
{
    return static_cast<Impl*>(OutChunk::pImpl.get())->to_string();
}

bool ProdInfo::operator ==(const ProdInfo& rhs) const noexcept
{
    return *static_cast<Impl*>(OutChunk::pImpl.get()) ==
            *static_cast<Impl*>(rhs.OutChunk::pImpl.get());
}

void ProdInfo::send(PeerProto& proto) const
{
    proto.send(*this);
}

void ProdInfo::save(Repository& repo) const
{
    throw LOGIC_ERROR("Not implemented yet");
    //repo.save(*this);
}

/******************************************************************************/

std::string SegId::to_string() const
{
    return "{prodId: " + prodIndex.to_string() + ", segOffset: " +
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
    return isProd
            ? id.prodIndex == rhs.id.prodIndex
            : id.segId == rhs.id.segId;
}

std::string ChunkId::to_string() const
{
    return isProd
            ? id.prodIndex.to_string()
            : id.segId.to_string();
}

void ChunkId::notify(PeerProto& peerProto) const
{
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

OutChunk ChunkId::get(Repository& repo) const
{
    if (isProd) {
        LOG_DEBUG("Getting information on product %s",
                id.prodIndex.to_string().data());
        return repo.getProdInfo(id.prodIndex);
    }
    else {
        LOG_DEBUG("Getting data-segment %s",
                id.segId.to_string().c_str());
        return repo.getMemSeg(id.segId);
    }
}

/******************************************************************************/

DataSeg::~DataSeg() noexcept
{}

/******************************************************************************/

class MemSeg::Impl final : public OutChunk::Impl
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

    ProdSize getSegOffset() const
    {
        return info.getId().getSegOffset();
    }

    bool operator ==(const Impl& rhs) const
    {
        return info == rhs.info &&
                ::memcmp(data, rhs.data, info.getSegSize()) == 0;
    }

    void writeData(
            void* const   data,
            const SegSize nbytes) const
    {
        if (nbytes > info.getSegSize())
            throw INVALID_ARGUMENT("Requested amount is greater than available "
                    "amount: {req: " + std::to_string(nbytes) + ", avail: " +
                    std::to_string(info.getSegSize()));
        ::memcpy(data, this->data, nbytes);
    }
};

MemSeg::MemSeg()
    : OutChunk{nullptr}
{}

MemSeg::MemSeg(
        const SegInfo& info,
        const void*    data)
    : OutChunk(new Impl(info, data))
{}

const SegInfo& MemSeg::getSegInfo() const noexcept
{
    return static_cast<Impl*>(pImpl.get())->getInfo();
}

const SegId& MemSeg::getSegId() const noexcept
{
    return static_cast<Impl*>(pImpl.get())->getSegId();
}

const void* MemSeg::getData() const
{
    return static_cast<Impl*>(pImpl.get())->getData();
}

SegSize MemSeg::getSegSize() const
{
    return static_cast<Impl*>(pImpl.get())->getSegSize();
}

ProdIndex MemSeg::getProdIndex() const noexcept
{
    return static_cast<Impl*>(pImpl.get())->getProdId();
}

ProdSize MemSeg::getProdSize() const
{
    return static_cast<Impl*>(pImpl.get())->getProdSize();
}

ProdSize MemSeg::getSegOffset() const noexcept
{
    return static_cast<Impl*>(pImpl.get())->getSegOffset();
}

bool MemSeg::operator ==(const MemSeg& rhs) const noexcept
{
    return *static_cast<Impl*>(pImpl.get()) ==
            *static_cast<Impl*>(rhs.pImpl.get());
}

void MemSeg::send(PeerProto& proto) const
{
    proto.send(*this);
}

ProdSize MemSeg::getOffset() const noexcept
{
    return getSegOffset();
}

void MemSeg::writeData(void* data, SegSize nbytes) const
{
    static_cast<Impl*>(pImpl.get())->writeData(data, nbytes);
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

    void write(void* buf)
    {
        const auto nbytes = getSegInfo().getSegSize();
        if (sock.read(buf, nbytes) != nbytes)
            throw EOF_ERROR();
    }

    ProdSize getOffset() const noexcept
    {
        return info.getId().getSegOffset();
    }

    void writeData(
            void*         buf,
            const SegSize nbytes)
    {
        if (sock.read(buf, nbytes) != nbytes)
            throw EOF_ERROR();
    }
};

UdpSeg::UdpSeg(
        const SegInfo& info,
        UdpSock&       sock)
    : SockSeg{new Impl(info, sock)}
{}

std::string UdpSeg::to_string() const
{
    return static_cast<Impl*>(pImpl.get())->to_string();
}

void UdpSeg::read(void* buf) const
{
    return static_cast<Impl*>(pImpl.get())->write(buf);
}

void UdpSeg::save(Repository& repo) const
{
    throw LOGIC_ERROR("Not implemented yet");
    //repo.save(*this);
}

ProdSize UdpSeg::getOffset() const noexcept
{
    return static_cast<Impl*>(pImpl.get())->getOffset();
}

void UdpSeg::writeData(
        void*   data,
        SegSize nbytes) const
{
    return static_cast<Impl*>(pImpl.get())->writeData(data, nbytes);
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

    void write(void* buf)
    {
        const auto nbytes = getSegInfo().getSegSize();
        if (sock.read(buf, nbytes) != nbytes)
            throw EOF_ERROR();
    }

    ProdSize getOffset() const noexcept
    {
        return info.getId().getSegOffset();
    }

    void writeData(
            void*         buf,
            const SegSize nbytes)
    {
        if (sock.read(buf, nbytes) != nbytes)
            throw EOF_ERROR();
    }
};

TcpSeg::TcpSeg(
        const SegInfo& info,
        TcpSock&       sock)
    : SockSeg{new Impl(info, sock)}
{}

std::string TcpSeg::to_string() const
{
    return static_cast<Impl*>(pImpl.get())->to_string();
}

void TcpSeg::read(void* buf) const
{
    return static_cast<Impl*>(pImpl.get())->write(buf);
}

void TcpSeg::save(Repository& repo) const
{
    throw LOGIC_ERROR("Not implemented yet");
    //repo.save(*this);
}

ProdSize TcpSeg::getOffset() const noexcept
{
    return static_cast<Impl*>(pImpl.get())->getOffset();
}

void TcpSeg::writeData(
        void*   data,
        SegSize nbytes) const
{
    return static_cast<Impl*>(pImpl.get())->writeData(data, nbytes);
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
