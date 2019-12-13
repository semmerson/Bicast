/**
 * Fundamental entities exchanged on a network.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Chunk.h
 *  Created on: Nov 22, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_PROTOCOL_CHUNK_H_
#define MAIN_PROTOCOL_CHUNK_H_

#include "PeerProto.h"
#include "McastProto.h"

#include <memory>

namespace hycast {

typedef uint16_t Flags;
typedef uint16_t SegSize;
typedef uint32_t ProdId;
typedef uint32_t ProdSize;

/******************************************************************************/

class ChunkId
{
protected:
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    ChunkId(const Impl* impl);

public:
    virtual ~ChunkId() noexcept;

    virtual std::string to_string() const =0;

    virtual size_t hash() const noexcept =0;

    virtual bool operator ==(const ChunkId& rhs) const noexcept =0;

    virtual bool operator ==(const InfoId& rhs) const noexcept =0;

    virtual bool operator ==(const SegId& rhs) const noexcept =0;

    virtual void notify(PeerProto& proto) const =0;

    virtual void request(PeerProto& proto) const =0;
};

/******************************************************************************/

class InfoId final : public ChunkId
{
    class Impl;

public:
    InfoId(ProdId prodIndex);

    ProdId getIndex() const noexcept;

    std::string to_string() const;

    size_t hash() const noexcept;

    bool operator ==(const ChunkId& rhs) const noexcept
    {
        return rhs == *this;
    }

    bool operator ==(const InfoId& rhs) const noexcept;

    bool operator ==(const SegId& rhs) const noexcept
    {
        return false;
    }

    void notify(PeerProto& proto) const;

    void request(PeerProto& proto) const;
};

/******************************************************************************/

class SegId final : public ChunkId
{
    class Impl;

public:
    SegId(  ProdId prodIndex,
            ProdSize  segOffset);

    ProdId getProdId() const noexcept;

    ProdSize getSegOffset() const noexcept;

    std::string to_string() const;

    size_t hash() const noexcept;

    bool operator ==(const ChunkId& rhs) const noexcept
    {
        return rhs == *this;
    }

    bool operator ==(const InfoId& rhs) const noexcept
    {
        return false;
    }

    bool operator ==(const SegId& rhs) const noexcept;

    void notify(PeerProto& proto) const;

    void request(PeerProto& proto) const;
};

/******************************************************************************/

class Info
{
public:
    virtual ~Info() =0;

    virtual ProdInfo& getInfo() const noexcept =0;
};

/******************************************************************************/

class Seg
{
public:
    virtual ~Seg() =0;

    virtual SegInfo& getInfo() const noexcept =0;
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
protected:
    class Impl;

    MemChunk(Impl* impl);

public:
    virtual ~MemChunk() =0;

    virtual void write(PeerProto& proto) const =0;

    virtual void write(McastSndr& proto) const =0;
};

class MemInfo final : public MemChunk, public Info
{
    class Impl;

public:
    MemInfo(const ProdInfo& info);
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
protected:
    class Impl;

    UdpChunk(Impl* impl);

public:
    virtual ~UdpChunk() =0;

    virtual void write(void* data);
};

class UdpInfo final : public UdpChunk, public Info
{
    class Impl;

public:
    UdpInfo(const ProdInfo& info,
            McastRcvr&      proto);
};

class UdpSeg final : public UdpChunk, public Seg
{
    class Impl;

public:
    UdpSeg( const SegInfo& info,
            McastRcvr&     proto);
};

/******************************************************************************/

class TcpChunk : public Chunk
{
protected:
    class Impl;

    TcpChunk(Impl* impl);

public:
    virtual ~TcpChunk() =0;

    virtual void write(void* data);
};

class TcpInfo final : public TcpChunk, public Info
{
    class Impl;

public:
    TcpInfo( const ProdInfo& info,
            PeerProto&      proto);
};

class TcpSeg final : public TcpChunk, public Seg
{
    class Impl;

public:
    TcpSeg( const SegInfo& info,
            PeerProto&     proto);
};

} // namespace

#endif /* MAIN_PROTOCOL_CHUNK_H_ */
