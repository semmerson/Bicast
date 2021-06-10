/**
 * The types involved in network exchanges.
 *
 *        File: hycast.cpp
 *  Created on: Oct 28, 2019
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

#include "config.h"

#include "hycast.h"

#include "error.h"
#include "Peer.h"
#include "Repository.h"

#include <cstring>
#include <sstream>

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

ProdInfo::ProdInfo() noexcept =default;

ProdInfo::ProdInfo(
        const ProdIndex    prodIndex,
        const ProdSize     prodSize,
        const std::string& prodName)
    : pImpl(new Impl(prodIndex, prodSize, prodName))
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

class SegData::Impl
{
protected:
    const SegSize segSize;

    Impl(const SegSize segSize)
        : segSize{segSize}
    {}

    virtual std::string getSubString() const =0;

public:
    virtual ~Impl() noexcept
    {}

    SegSize getSegSize() const noexcept
    {
        return segSize;
    }

    virtual void getData(void* buf) =0;

    std::string to_string() const
    {
        return "{segSize: " + std::to_string(segSize) + ", " + getSubString() +
                "}";
    }
};

SegData::SegData(Impl* impl)
    : pImpl{impl}
{}

SegData::~SegData() noexcept
{}

SegSize SegData::getSegSize() const noexcept
{
    return pImpl->getSegSize();
}

std::string SegData::to_string() const
{
    return pImpl->to_string();
}

void SegData::getData(void* buf)
{
    pImpl->getData(buf);
}

/******************************************************************************/

class MemSegData::Impl final : public SegData::Impl
{
    const void* bytes;

protected:
    std::string getSubString() const
    {
        std::ostringstream buf;
        buf << "data: " << bytes;
        return buf.str();
    }

public:
    Impl(   const void*   bytes,
            const SegSize segSize)
        : SegData::Impl{segSize}
        , bytes(bytes)
    {}

    void getData(void* buf)
    {
        ::memcpy(buf, bytes, segSize);
    }

    const void* data() const noexcept
    {
        return bytes;
    }

    bool operator ==(const Impl& rhs)
    {
        return (segSize == rhs.segSize) &&
                (::memcmp(bytes, rhs.bytes, segSize) == 0);
    }
};

MemSegData::MemSegData(
        const void*   data,
        const SegSize segSize)
    : SegData{new Impl(data, segSize)}
{}

const void* MemSegData::data() const noexcept
{
    return static_cast<Impl*>(pImpl.get())->data();
}

/******************************************************************************/

class TcpSegData::Impl final : public SegData::Impl
{
    TcpSock sock;

protected:
    std::string getSubString() const
    {
        return "sock: " + sock.to_string();
    }

public:
    Impl(   TcpSock& sock,
            const SegSize segSize)
        : SegData::Impl{segSize}
        , sock{sock}
    {}

    void getData(void* buf)
    {
        sock.read(buf, segSize);
    }
};

TcpSegData::TcpSegData(
        TcpSock&      sock,
        const SegSize segSize)
    : SegData{new Impl(sock, segSize)}
{}

/******************************************************************************/

class UdpSegData::Impl final : public SegData::Impl
{
    UdpSock sock;

protected:
    std::string getSubString() const
    {
        return "sock: " + sock.to_string();
    }

public:
    Impl(   UdpSock&      sock,
            const SegSize segSize)
        : SegData::Impl{segSize}
        , sock{sock}
    {}

    void getData(void* buf)
    {
        sock.addPeek(buf, segSize);
        sock.peek();
    }
};

UdpSegData::UdpSegData(
        UdpSock&      sock,
        const SegSize segSize)
    : SegData{new Impl(sock, segSize)}
{}

/******************************************************************************/

class DataSeg::Impl
{
protected:
    const SegInfo segInfo;
    SegData       segData;

    Impl(const SegInfo& info,
         SegData&       data)
        : segInfo{info}
        , segData{data}
    {}

    Impl(const SegInfo& info,
         SegData&&      data)
        : segInfo{info}
        , segData{data}
    {}

public:
    virtual ~Impl() noexcept
    {}

    void getData(void* buf)
    {
        segData.getData(buf);
    }

    const SegInfo& getSegInfo() const
    {
        return segInfo;
    }

    const SegId& getSegId() const noexcept
    {
        return segInfo.getSegId();
    }

    const SegSize getSegSize() const noexcept
    {
        return segInfo.getSegSize();
    }

    ProdIndex getProdIndex() const noexcept
    {
        return segInfo.getProdIndex();
    }

    ProdSize getProdSize() const noexcept
    {
        return segInfo.getProdSize();
    }

    ProdSize getSegOffset() const noexcept
    {
        return segInfo.getSegOffset();
    }

    std::string to_string(const bool withName) const
    {
        std::string string;
        if (withName)
            string += "DataSeg";
        return string += "{info=" + segInfo.to_string() + ", data=" +
                segData.to_string() + "}";
    }
};

DataSeg::DataSeg(Impl* impl)
    : pImpl{impl}
{}

DataSeg::~DataSeg() noexcept
{}

DataSeg::operator bool() const noexcept
{
    return static_cast<bool>(pImpl);
}

void DataSeg::getData(void* buf) const
{
    pImpl->getData(buf);
}

const SegInfo& DataSeg::getSegInfo() const
{
    return pImpl->getSegInfo();
}

const SegId& DataSeg::getSegId() const noexcept
{
    return pImpl->getSegId();
}

SegSize DataSeg::getSegSize() const noexcept
{
    return pImpl->getSegSize();
}

ProdIndex DataSeg::getProdIndex() const noexcept
{
    return pImpl->getProdIndex();
}

ProdSize DataSeg::getProdSize() const noexcept
{
    return pImpl->getProdSize();
}

ProdSize DataSeg::getSegOffset() const noexcept
{
    return pImpl->getSegOffset();
}

std::string DataSeg::to_string(const bool withName) const
{
    return pImpl->to_string(withName);
}

/******************************************************************************/

class MemSeg::Impl : public DataSeg::Impl
{
public:
    Impl(   const SegInfo& info,
            const void*    data)
        : DataSeg::Impl{info, MemSegData(data, info.getSegSize())}
    {}

    const void* data() const
    {
        return reinterpret_cast<const MemSegData*>(&segData)->data();
    }

    bool operator ==(const Impl& rhs) const
    {
        return (segInfo == rhs.segInfo) &&
                (::memcmp(data(), rhs.data(), segInfo.getSegSize()) == 0);
    }
};

MemSeg::MemSeg()
{}

MemSeg::MemSeg(
        const SegInfo& info,
        const void*    data)
    : DataSeg(new Impl(info, data))
{}

MemSeg::operator bool() const noexcept {
    return static_cast<bool>(pImpl);
}

const void* MemSeg::data() const
{
    return static_cast<Impl*>(pImpl.get())->data();
}

bool MemSeg::operator ==(const MemSeg& rhs) const noexcept
{
    return (pImpl.get() == rhs.pImpl.get()) ||
           (static_cast<Impl*>(pImpl.get())->operator ==(
            *static_cast<Impl*>(rhs.pImpl.get())));
}

/******************************************************************************/

class UdpSeg::Impl final : public DataSeg::Impl
{
public:
    Impl(   const SegInfo& info,
            UdpSock&       sock)
        : DataSeg::Impl{info, UdpSegData(sock, info.getSegSize())}
    {}
};

UdpSeg::UdpSeg(
        const SegInfo& info,
        UdpSock&       sock)
    : DataSeg{new Impl(info, sock)}
{}

/******************************************************************************/

class TcpSeg::Impl final : public DataSeg::Impl
{
public:
    Impl(   const SegInfo& info,
            TcpSock&       sock)
        : DataSeg::Impl{info, TcpSegData(sock, info.getSegSize())}
    {}
};

TcpSeg::TcpSeg(
        const SegInfo& info,
        TcpSock&       sock)
    : DataSeg{new Impl(info, sock)}
{}

} // namespace

namespace std {
    string to_string(const hycast::ProdInfo& prodInfo)
    {
        return prodInfo.to_string();
    }
}
