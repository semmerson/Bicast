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

#include "error.h"
#include "hycast.h"

namespace hycast {

class ProdInfo::Impl {
    const ProdIndex   index;
    const ProdSize    size;
    const std::string name;

public:
    Impl(   const ProdIndex    index,
            const ProdSize     size,
            const std::string& name)
        : index{index}
        , size{size}
        , name{name}
    {}

    ProdIndex getIndex() const
    {
        return index;
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
        return index == rhs.index &&
               size == rhs.size &&
               name == rhs.name;
    }
};

/******************************************************************************/

ProdInfo::ProdInfo(Impl* const impl)
    : pImpl{impl}
{}

ProdInfo::ProdInfo(
        const ProdIndex    index,
        const ProdSize     size,
        const std::string& name)
    : pImpl{new Impl(index, size, name)}
{}

ProdIndex ProdInfo::getIndex() const
{
    return pImpl->getIndex();
}

ProdSize ProdInfo::getSize() const
{
    return pImpl->getSize();
}

const std::string& ProdInfo::getName() const
{
    return pImpl->getName();
}

bool ProdInfo::operator ==(const ProdInfo& rhs) const
{
    return pImpl->operator ==(*rhs.pImpl.get());
}

/******************************************************************************/

std::string SegId::to_string() const
{
    return "{prodIndex: " + std::to_string(prodIndex) + ", segOffset: " +
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

    ProdIndex getProdIndex() const
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
};

MemSeg::MemSeg(Impl* impl)
    : pImpl{impl}
{}

MemSeg::MemSeg(
        const SegInfo& info,
        const void*    data)
    : MemSeg{new Impl(info, data)}
{}

const SegInfo& MemSeg::getInfo() const
{
    return pImpl->getInfo();
}

const void* MemSeg::getData() const
{
    return pImpl->getData();
}

SegSize MemSeg::getSegSize() const
{
    return pImpl->getSegSize();
}

ProdIndex MemSeg::getProdIndex() const
{
    return pImpl->getProdIndex();
}

ProdSize MemSeg::getProdSize() const
{
    return pImpl->getProdSize();
}

ProdSize MemSeg::getSegOffset() const
{
    return pImpl->getSegOffset();
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

    virtual void write(void* data) =0;
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

    void write(void* data)
    {
        auto nbytes = info.getSegSize();
        auto nread = sock.read(data, info.getSegSize());

        if (nread != nbytes)
            throw RUNTIME_ERROR("Couldn't read segment data");
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

void UdpSeg::write(void* data) const
{
    static_cast<Impl*>(pImpl.get())->write(data);
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

    void write(void* data)
    {
        sock.read(data, info.getSegSize());
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

void TcpSeg::write(void* data) const
{
    static_cast<Impl*>(pImpl.get())->write(data);
}

} // namespace

hycast::SockSeg::~SockSeg() {
}
