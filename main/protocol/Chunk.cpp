/**
 * Fundamental entities exchanged on a network.
 *
 *        File: Chunk.cpp
 *  Created on: May 10, 2019
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

#include "Chunk.h"
#include "error.h"

namespace hycast {

struct BopIdMsg {
    Flags     flags;
    SegSize   pad;
    ProdId prodIndex;
};

struct SegIdMsg {
    Flags     flags;
    SegSize   pad;
    ProdId prodIndex;
    ProdSize  segOffset;
};

struct BopMsg {
    Flags     flags;
    SegSize   nameLen;
    ProdId prodIndex;
    ProdSize  prodSize;
    char      name[];
};

struct SegMsg {
    Flags     flags;
    SegSize   segSize;
    ProdId prodIndex;
    ProdSize  prodSize;
    ProdSize  segOffset;
    char      data[];
};

/******************************************************************************/

class ChunkId::Impl
{
public:
    virtual ~Impl() noexcept
    {}
};

ChunkId::ChunkId(const Impl* impl)
    : pImpl{impl}
{}

/******************************************************************************/

class BopId::Impl : public ChunkId::Impl
{
    ProdId index;

protected:
    bool operator ==(const Impl& rhs) const noexcept
    {
        return index == rhs.index;
    }

public:
    Impl(ProdId index)
        : index{index}
    {}

    ProdId getIndex() const noexcept
    {
        return index;
    }

    std::string to_string() const
    {
        return std::to_string(index);
    }

    size_t hash() const noexcept
    {
        return std::hash<ProdId>(index);
    }
};

BopId::BopId(ProdId prodIndex)
    : ChunkId{new Impl(prodIndex)}
{}

ProdId BopId::getIndex() const noexcept
{
    return static_cast<BopId::Impl*>(pImpl.get())->getIndex();
}

std::string BopId::to_string() const
{
    return static_cast<BopId::Impl*>(pImpl.get())->to_string();
}

size_t BopId::hash() const noexcept
{
    return static_cast<BopId::Impl*>(pImpl.get())->hash();
}

bool BopId::operator ==(const BopId& rhs) const noexcept
{
    return *static_cast<BopId::Impl*>(pImpl.get()) ==
           *static_cast<BopId::Impl*>(rhs.pImpl.get());
}

void BopId::write(McastSndr& proto) const
{
    proto->write(*this);
}

void BopId::write(PeerProto& proto) const
{
    proto->write(*this);
}

/******************************************************************************/

class SegId final : public ChunkId
{
    class Impl;

protected:
    bool operator ==(const BopId& rhs) const noexcept
    {
        return false;
    }

    bool operator ==(const SegId& rhs) const noexcept;

public:
    SegId(  ProdId prodIndex,
            ProdSize  segOffset);

    ProdId getProdIndex() const noexcept;

    ProdSize getSegOffset() const noexcept;

    std::string to_string() const;

    size_t hash() const noexcept;

    bool operator ==(const ChunkId& rhs) const noexcept
    {
        return rhs == *this;
    }

    void write(McastSndr* sock) const;

    void write(PeerProto* sock) const;
};

/******************************************************************************/

class Bop
{
public:
    virtual ~Bop() =0;

    virtual ProdInfo& getInfo() const noexcept =0;
};

/******************************************************************************/

class Seg
{
public:
    virtual ~Seg() =0;

    virtual SegInfo& getInfo() const noexcept =0;

    virtual void write(void* data);
};

/******************************************************************************/

class Chunk
{
protected:
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    Chunk(Impl* impl);

public:
    virtual ~Chunk() noexcept =0;

    virtual ChunkId& getId() const noexcept =0;
};

/******************************************************************************/

class MemChunk : public Chunk
{
public:
    virtual ~MemChunk() =0;

    virtual void write(McastSndr& sock) const =0;

    virtual void write(PeerProto& sock) const =0;
};

class MemBop final : public MemChunk, public Bop
{
    class Impl;

public:
    MemBop(const ProdInfo& info);
};

class MemSeg final : public MemChunk, public Seg
{
    class Impl;

public:
    MemSeg( const SegInfo& info,
            const void*    data);
};

/******************************************************************************/

class UdpChunk : public Chunk
{
public:
    virtual ~UdpChunk() =0;
};

class UdpBop final : public UdpChunk, public Bop
{
    class Impl;

public:
    UdpBop( const ProdInfo& info,
            McastSndr&        sock);
};

class UdpSeg final : public UdpChunk, public Seg
{
    class Impl;

public:
    UdpSeg( const SegInfo& info,
            McastSndr&       sock);
};

/******************************************************************************/

class TcpChunk : public Chunk
{
public:
    virtual ~TcpChunk() =0;
};

class TcpBop final : public TcpChunk, public Bop
{
    class Impl;

public:
    TcpBop( const ProdInfo& info,
            PeerProto&        sock);
};

class TcpSeg final : public TcpChunk, public Seg
{
    class Impl;

public:
    TcpSeg( const SegInfo& info,
            PeerProto&       sock);
};

} // namespace
