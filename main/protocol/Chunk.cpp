/**
 * Fundamental entities exchanged on a network.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Chunk.cpp
 *  Created on: May 10, 2019
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "Chunk.h"
#include "error.h"

namespace hycast {

struct BopIdMsg {
    Flags     flags;
    SegSize   pad;
    ProdIndex prodIndex;
};

struct SegIdMsg {
    Flags     flags;
    SegSize   pad;
    ProdIndex prodIndex;
    ProdSize  segOffset;
};

struct BopMsg {
    Flags     flags;
    SegSize   nameLen;
    ProdIndex prodIndex;
    ProdSize  prodSize;
    char      name[];
};

struct SegMsg {
    Flags     flags;
    SegSize   segSize;
    ProdIndex prodIndex;
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
    ProdIndex index;

protected:
    bool operator ==(const Impl& rhs) const noexcept
    {
        return index == rhs.index;
    }

public:
    Impl(ProdIndex index)
        : index{index}
    {}

    ProdIndex getIndex() const noexcept
    {
        return index;
    }

    std::string to_string() const
    {
        return std::to_string(index);
    }

    size_t hash() const noexcept
    {
        return std::hash<ProdIndex>(index);
    }
};

BopId::BopId(ProdIndex prodIndex)
    : ChunkId{new Impl(prodIndex)}
{}

ProdIndex BopId::getIndex() const noexcept
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
    SegId(  ProdIndex prodIndex,
            ProdSize  segOffset);

    ProdIndex getProdIndex() const noexcept;

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
