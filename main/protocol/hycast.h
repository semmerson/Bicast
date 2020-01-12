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

#include "error.h"
#include "Socket.h"

#include <climits>
#include <memory>
#include <string>

/******************************************************************************/

namespace hycast {

typedef uint16_t SegSize;
typedef uint32_t ProdSize;

/******************************************************************************/

class ProdIndex
{
public:
    typedef uint32_t ValueType;

    ProdIndex() noexcept
        : index{0}
    {}

    ProdIndex(const ValueType index)
        : index{index}
    {
        if (index == 0)
            throw INVALID_ARGUMENT("Can't explicitly initialize index to zero");
    }

    ProdIndex& operator =(const ProdIndex& rhs)
    {
        index = rhs.index;
        return *this;
    }

    operator bool() const noexcept
    {
        return index != 0;
    }

    ValueType getValue() const noexcept
    {
        return index;
    }

    size_t hash() const noexcept
    {
        return index;
    }

    std::string to_string() const;

    bool operator ==(const ProdIndex rhs) const noexcept
    {
        return index == rhs.index;
    }

private:
    ValueType index;
};

/******************************************************************************/

class Flags
{
public:
    typedef uint16_t flags_t;

private:
    static const flags_t SEG = 0;  // Identifies data-segment chunk
    static const flags_t PROD = 1; // Identifies product chunk

public:
    inline static bool isProd(flags_t val)
    {
        return val & PROD;
    }

    inline static bool isSeg(flags_t val)
    {
        return !isProd(val);
    }

    inline static flags_t getProd()
    {
        return PROD;
    }

    inline static flags_t getSeg()
    {
        return SEG;
    }
};

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
protected:
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    OutChunk(Impl* impl);

public:
    virtual ~OutChunk() noexcept
    {}

    operator bool() const
    {
        return static_cast<bool>(pImpl);
    }

    virtual void send(PeerProto& proto) const;
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
     * Default constructs. The instance will be invalid.
     *
     * @see `operator bool()`
     */
    ProdInfo();

    /**
     * Constructs a valid instance.
     *
     * @param[in] prodIndex  Product index
     * @param[in] size       Product size in bytes
     * @param[in] name       Product name
     */
    ProdInfo(
            ProdIndex          prodIndex,
            ProdSize           size,
            const std::string& name);

    /**
     * Indicates if an instance is valid.
     *
     * @retval `false`  Invalid
     * @retval `true`   Valid
     */
    operator bool() const noexcept;

    ProdIndex getProdIndex() const;

    ProdSize getSize() const;

    const std::string& getName() const;

    std::string to_string() const;

    bool operator ==(const ProdInfo& rhs) const noexcept;

    void send(PeerProto& proto) const;

    void save(Repository& repo) const;
};

/******************************************************************************/

/**
 * Data-segment identifier.
 */
class SegId
{
    const ProdIndex prodIndex;
    const ProdSize  segOffset;

public:
    SegId(  const ProdIndex prodIndex,
            const ProdSize  segOffset)
        : prodIndex{prodIndex}
        , segOffset{segOffset}
    {}

    SegId()
        : prodIndex{}
        , segOffset{0}
    {}

    ProdIndex getProdIndex() const noexcept
    {
        return prodIndex;
    }

    ProdSize getSegOffset() const noexcept
    {
        return segOffset;
    }

    size_t hash() const noexcept
    {
        return prodIndex.hash() ^
                std::hash<ProdSize>()(segOffset);
    }

    bool operator ==(const SegId& rhs) const noexcept
    {
        return prodIndex == rhs.prodIndex &&
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
        ProdIndex prodIndex;
        SegId     segId;
    } id;

public:
    ChunkId(ProdIndex prodIndex)
        : isProd{true}
        , id{.prodIndex=prodIndex}
    {}

    ChunkId(const SegId segId)
        : isProd{false}
        , id{.segId=segId}
    {}

    ChunkId()
        : ChunkId(SegId{})
    {}

    bool operator ==(const ChunkId& rhs) const;

    bool isProdIndex() const
    {
        return isProd;
    }

    ProdIndex getProdIndex() const
    {
        return id.prodIndex;
    }

    SegId getSegId() const
    {
        return id.segId;
    }

    std::string to_string() const;

    void notify(PeerProto& peerProto) const;

    void request(PeerProto& peerProto) const;

    OutChunk get(Repository& repo) const;
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

    const SegId& getId() const noexcept
    {
        return id;
    }

    ProdIndex getProdIndex() const noexcept
    {
        return id.getProdIndex();
    }

    ProdSize getProdSize() const noexcept
    {
        return prodSize;
    }

    SegSize getSegSize() const noexcept
    {
        return segSize;
    }

    bool operator ==(const SegInfo& rhs) const
    {
        return id == rhs.id &&
                prodSize == rhs.prodSize &&
                segSize == rhs.segSize;
    }
};

/******************************************************************************/

/**
 * Interface for a data-segment.
 */
class DataSeg
{
public:
    virtual ~DataSeg() noexcept;

    virtual const SegInfo& getSegInfo() const =0;

    virtual const SegId& getSegId() const noexcept =0;

    virtual ProdIndex getProdIndex() const noexcept =0;

    virtual ProdSize getOffset() const noexcept =0;

    virtual void writeData(void* data, SegSize nbytes) const =0;
};

/******************************************************************************/

/**
 * Data-segment that resides in memory.
 */
class MemSeg final : public OutChunk, public DataSeg
{
    class Impl;

public:
    MemSeg();

    MemSeg(const SegInfo& info,
           const void*    data);

    const SegInfo& getSegInfo() const noexcept;

    const SegId& getSegId() const noexcept;

    const void* getData() const;

    SegSize getSegSize() const;

    ProdIndex getProdIndex() const noexcept;

    ProdSize getProdSize() const;

    ProdSize getSegOffset() const noexcept;

    bool operator ==(const MemSeg& rhs) const noexcept;

    void send(PeerProto& proto) const;

    ProdSize getOffset() const noexcept override;

    void writeData(void* data, SegSize nbytes) const override;
};

/******************************************************************************/

/**
 * Abstract, socket-based data-segment.
 */
class SockSeg : public DataSeg
{
protected:
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    SockSeg(Impl* impl);

public:
    virtual ~SockSeg();

    const SegInfo& getSegInfo() const noexcept;

    const SegId& getSegId() const noexcept;

    ProdIndex getProdIndex() const noexcept;

    virtual std::string to_string() const =0;

    virtual void read(void* buf) const =0;

    virtual void save(Repository& repo) const =0;

    virtual ProdSize getOffset() const noexcept =0;

    virtual void writeData(
            void*   data,
            SegSize nbytes) const =0;
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

    std::string to_string() const override;

    void read(void* buf) const override;

    void save(Repository& repo) const override;

    ProdSize getOffset() const noexcept;

    void writeData(void* data, SegSize nbytes) const;
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

    std::string to_string() const override;

    void read(void* buf) const override;

    void save(Repository& repo) const override;

    ProdSize getOffset() const noexcept;

    void writeData(void* data, SegSize nbytes) const;
};

} // namespace

/******************************************************************************/

namespace std {
    string to_string(const hycast::ProdInfo& prodInfo);

    template<>
    struct hash<hycast::ProdIndex>
    {
        inline size_t operator ()(const hycast::ProdIndex& prodIndex) const
        {
            return prodIndex.hash();
        }
    };

    template<>
    struct hash<hycast::ChunkId>
    {
        inline size_t operator ()(const hycast::ChunkId& chunkId) const
        {
            return chunkId.isProd
                    ? chunkId.id.prodIndex.hash()
                    : chunkId.id.segId.hash();
        }
    };
}

#endif /* MAIN_HYCAST_H_ */
