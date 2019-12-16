/**
 * The types involved in network exchanges.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: hycast.h
 *  Created on: May 10, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_HYCAST_H_
#define MAIN_HYCAST_H_

#include "Socket.h"

#include <climits>
#include <memory>
#include <string>

/******************************************************************************/

namespace hycast {

typedef uint16_t Flags;
typedef uint16_t SegSize;
typedef uint32_t ProdId;
typedef uint32_t ProdSize;

/******************************************************************************/

class PeerProto;
class Repository;

class Chunk
{
public:
    class Impl;

protected:
    std::shared_ptr<Impl> pImpl;

    Chunk()
    {}

    Chunk(Impl* impl);

public:
    virtual ~Chunk() noexcept;

    operator bool() const
    {
        return (bool)pImpl;
    }
};

/**
 * A chunk that is sent to a remote peer.
 */
class OutChunk
{
public:
    class Impl;

protected:
    std::shared_ptr<Impl> pImpl;

    OutChunk(Impl* impl);

public:
    virtual ~OutChunk() noexcept
    {}

    operator bool() const
    {
        return (bool)pImpl;
    }

    virtual void send(PeerProto& proto) const =0;
};

/**
 * A chunk that is received from a remote peer.
 */
class InChunk
{
public:
    class Impl;

protected:
    std::shared_ptr<Impl> pImpl;

    InChunk(); // For `prodInfo` construction

    InChunk(Impl* impl);

public:
    virtual ~InChunk() noexcept
    {}

    virtual void save(Repository& repo) const =0;
};

/******************************************************************************/

class ProdInfo final : virtual public OutChunk, virtual public InChunk
{
    class Impl;

public:
    /**
     * Constructs.
     *
     * @param[in] prodId  Product identifier
     * @param[in] size    Product size in bytes
     * @param[in] name    Product name
     */
    ProdInfo(
            ProdId             prodId,
            ProdSize           size,
            const std::string& name);

    ProdId getIndex() const;

    ProdSize getSize() const;

    const std::string& getName() const;

    bool operator ==(const ProdInfo& rhs) const;

    void send(PeerProto& proto) const;

    void save(Repository& repo) const;
};

/******************************************************************************/

/**
 * Data-segment identifier.
 */
class SegId
{
    const ProdId   prodId;
    const ProdSize segOffset;

public:
    SegId(  const ProdId prodId,
            const ProdSize  segOffset)
        : prodId{prodId}
        , segOffset{segOffset}
    {}

    SegId()
        : SegId(0, 0)
    {}

    ProdId getProdId() const noexcept
    {
        return prodId;
    }

    ProdSize getSegOffset() const noexcept
    {
        return segOffset;
    }

    size_t hash() const noexcept
    {
        return std::hash<ProdId>()(prodId) ^
                std::hash<ProdSize>()(segOffset);
    }

    bool operator ==(const SegId& rhs) const noexcept
    {
        return prodId == rhs.prodId &&
                segOffset == rhs.segOffset;
    }

    std::string to_string() const;
};

/******************************************************************************/

class PeerProto;

/**
 * An identifier of a chunk.
 */
class ChunkId final
{
    friend class std::hash<ChunkId>;

    /*
     * Implemented as a discriminated union in order to have a fixed size in
     * a container.
     */
    bool isProd;

    union {
        ProdId prodId;
        SegId  segId;
    } id;

public:
    ChunkId(ProdId prodId)
        : isProd{true}
        , id{.prodId=prodId}
    {}

    ChunkId(const SegId segId)
        : isProd{false}
        , id{.segId=segId}
    {}

    ChunkId()
        : ChunkId(SegId{})
    {}

    bool operator ==(const ChunkId& rhs) const;

    bool isProdId() const
    {
        return isProd;
    }

    ProdId getProdId() const
    {
        return id.prodId;
    }

    SegId getSegId() const
    {
        return id.segId;
    }

    std::string to_string() const;

    void notify(PeerProto& peerProto) const;

    void request(PeerProto& peerProto) const;
};

/******************************************************************************/

/**
 * Data-segment information.
 */
class SegInfo
{
    const SegId     id;
    const ProdSize  prodSize;
    const SegSize   segSize;

public:
    SegInfo(const SegId    id,
            const ProdSize prodSize,
            const SegSize  segSize)
        : id{id}
        , prodSize{prodSize}
        , segSize{segSize}
    {}

    std::string to_string() const;

    const SegId& getId() const
    {
        return id;
    }

    ProdSize getProdSize() const
    {
        return prodSize;
    }

    SegSize getSegSize() const
    {
        return segSize;
    }
};

/******************************************************************************/

/**
 * Data-segment that resides in memory.
 */
class MemSeg final : public OutChunk
{
    class Impl;

public:
    MemSeg(const SegInfo& info,
           const void*    data);

    const SegInfo& getInfo() const;

    const void* getData() const;

    SegSize getSegSize() const;

    ProdId getProdIndex() const;

    ProdSize getProdSize() const;

    ProdSize getSegOffset() const;

    void send(PeerProto& proto) const;
};

/******************************************************************************/

/**
 * Abstract, socket-based data-segment.
 */
class SockSeg
{
protected:
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    SockSeg(Impl* impl);

public:
    virtual ~SockSeg();

    const SegInfo& getInfo() const;

    const SegId& getId() const;

    virtual std::string to_string() const =0;

    virtual void save(Repository& repo) const =0;
};

/**
 * Data-segment from a UDP socket.
 */
class UdpSeg final : public SockSeg
{
    class Impl;

public:
    UdpSeg( const SegInfo& info,
            UdpSock&       sock);

    std::string to_string() const;

    void write(void* buf) const;

    void save(Repository& repo) const override;
};

/**
 * Data-segment from a TCP socket.
 */
class TcpSeg final : public SockSeg
{
    class Impl;

public:
    TcpSeg( const SegInfo& info,
            TcpSock&       sock);

    std::string to_string() const;

    void write(void* buf) const;

    void save(Repository& repo) const override;
};

} // namespace

/******************************************************************************/

namespace std {
    template<>
    struct hash<hycast::ChunkId>
    {
        size_t operator ()(const hycast::ChunkId& chunkId) const
        {
            return chunkId.isProd
                    ? std::hash<hycast::ProdId>()(chunkId.id.prodId)
                    : chunkId.id.segId.hash();
        }
    };
}

#endif /* MAIN_HYCAST_H_ */
