/**
 * The types involved in network exchanges.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
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

namespace hycast {

class OutChunk::Impl
{};

OutChunk::OutChunk(Impl* impl)
    : pImpl{impl}
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
    const ProdId      prodId;
    const ProdSize    size;
    const std::string name;

public:
    Impl(   const ProdId       prodId,
            const ProdSize     size,
            const std::string& name)
        : prodId{prodId}
        , size{size}
        , name{name}
    {}

    ProdId getIndex() const
    {
        return prodId;
    }

    ProdSize getSize() const
    {
        return size;
    }

    const std::string& getName() const
    {
        return name;
    }

    bool operator ==(const Impl& rhs) const
    {
        return prodId == rhs.prodId &&
               size == rhs.size &&
               name == rhs.name;
    }
};

/******************************************************************************/

/******************************************************************************/

ProdInfo::ProdInfo(
        const ProdId       prodId,
        const ProdSize     size,
        const std::string& name)
    : OutChunk(new Impl(prodId, size, name))
{}

ProdId ProdInfo::getIndex() const
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

bool ProdInfo::operator ==(const ProdInfo& rhs) const
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
    return "{prodId: " + std::to_string(prodId) + ", segOffset: " +
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
            ? id.prodId == rhs.id.prodId
            : id.segId == rhs.id.segId;
}

std::string ChunkId::to_string() const
{
    return isProd
            ? std::to_string(id.prodId)
            : id.segId.to_string();
}

void ChunkId::notify(PeerProto& peerProto) const
{
    if (isProd) {
        LOG_DEBUG("Notifying about product %lu",
                static_cast<unsigned long>(id.prodId));
        peerProto.notify(id.prodId);
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
        LOG_DEBUG("Requesting information about product %lu",
                static_cast<unsigned long>(id.prodId));
        peerProto.request(id.prodId);
    }
    else {
        LOG_DEBUG("Requesting segment %s",
                id.segId.to_string().c_str());
        peerProto.request(id.segId);
    }
}

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

    const SegInfo& getInfo() const
    {
        return info;
    }

    const void* getData() const
    {
        return data;
    }

    SegSize getSegSize() const
    {
        return info.getSegSize();
    }

    ProdId getProdIndex() const
    {
        return info.getId().getProdId();
    }

    ProdSize getProdSize() const
    {
        return info.getProdSize();
    }

    ProdSize getSegOffset() const
    {
        return info.getId().getSegOffset();
    }
};

MemSeg::MemSeg(
        const SegInfo& info,
        const void*    data)
    : OutChunk(new Impl(info, data))
{}

const SegInfo& MemSeg::getInfo() const
{
    return static_cast<Impl*>(pImpl.get())->getInfo();
}

const void* MemSeg::getData() const
{
    return static_cast<Impl*>(pImpl.get())->getData();
}

SegSize MemSeg::getSegSize() const
{
    return static_cast<Impl*>(pImpl.get())->getSegSize();
}

ProdId MemSeg::getProdIndex() const
{
    return static_cast<Impl*>(pImpl.get())->getProdIndex();
}

ProdSize MemSeg::getProdSize() const
{
    return static_cast<Impl*>(pImpl.get())->getProdSize();
}

ProdSize MemSeg::getSegOffset() const
{
    return static_cast<Impl*>(pImpl.get())->getSegOffset();
}

void MemSeg::send(PeerProto& proto) const
{
    proto.send(*this);
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

    const SegInfo& getInfo() const
    {
        return info;
    }

    const SegId& getId() const
    {
        return info.getId();
    }

    virtual std::string to_string() const =0;
};

SockSeg::SockSeg(Impl* impl)
    : pImpl{impl}
{}

const SegInfo& SockSeg::getInfo() const
{
    return pImpl->getInfo();
}

const SegId& SockSeg::getId() const
{
    return pImpl->getId();
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
        const auto nbytes = getInfo().getSegSize();
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

void UdpSeg::write(void* buf) const
{
    return static_cast<Impl*>(pImpl.get())->write(buf);
}

void UdpSeg::save(Repository& repo) const
{
    throw LOGIC_ERROR("Not implemented yet");
    //repo.save(*this);
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
        const auto nbytes = getInfo().getSegSize();
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

void TcpSeg::write(void* buf) const
{
    return static_cast<Impl*>(pImpl.get())->write(buf);
}

void TcpSeg::save(Repository& repo) const
{
    throw LOGIC_ERROR("Not implemented yet");
    //repo.save(*this);
}

} // namespace

hycast::SockSeg::~SockSeg() {
}
