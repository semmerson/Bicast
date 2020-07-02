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

#include <main/inet/Socket.h>
#include "error.h"
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
    typedef uint32_t Type;

    ProdIndex() noexcept
        : index{0} // Invalid index
    {}

    ProdIndex(const Type index)
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

    operator Type() const noexcept
    {
        return index;
    }

    Type getValue() const noexcept
    {
        return index;
    }

    size_t hash() const noexcept
    {
        return index;
    }

    std::string to_string() const;

    ProdIndex operator ++() noexcept
    {
        if (++index == 0)
            index = 1;
        return *this;
    }

    bool operator ==(const ProdIndex rhs) const noexcept
    {
        return index == rhs.index;
    }

private:
    Type index;
};

/******************************************************************************/

class Flags
{
public:
    typedef uint16_t Type;

    typedef enum {
        PROD_INFO_NOTICE = 1,
        DATA_SEG_NOTICE,
        PROD_INFO_REQUEST,
        DATA_SEG_REQUEST,
        PROD_INFO,
        DATA_SEG,
        PATH_TO_SRC,
        NO_PATH_TO_SRC
    } MsgId;

private:
    static const Type PROD = 1;     // Identifies product-related chunk
    static const Type SRC_PATH = 2; // Have path to source of data-products?

    std::atomic<Type> flags;

public:
    Flags() noexcept;

    Flags(const Type flags) noexcept;

    Flags(const Flags& flags) noexcept;

    Flags& operator =(const Type flags) noexcept;

    operator Type() const noexcept;

    Flags& setPathToSrc() noexcept;

    Flags& clrPathToSrc() noexcept;

    bool isPathToSrc() const noexcept;

    Flags& setProd() noexcept;

    bool isProd() const noexcept;

    inline static bool isProd(Type val)
    {
        return val & PROD;
    }
};

/******************************************************************************/

class SubPeer;
class PeerProto;
class SubPeerProto;
class Repository;

/******************************************************************************/

class ProdInfo
{
    class Impl;

    std::shared_ptr<Impl> pImpl;

public:
    /**
     * Default constructs. The instance will test false.
     *
     * @see `operator bool()`
     */
    ProdInfo() noexcept;

    /**
     * Constructs.
     *
     * @param[in] prodIndex        Product index
     * @param[in] size             Product size in bytes
     * @param[in] name             Product name
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

    ProdSize getProdSize() const;

    const std::string& getProdName() const;

    std::string to_string() const;

    bool operator ==(const ProdInfo& rhs) const noexcept;

    void send(PeerProto& proto) const;
};

/******************************************************************************/

/**
 * Data-segment identifier.
 */
class SegId
{
    ProdIndex prodIndex;
    ProdSize  segOffset;

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

    SegId(const SegId& segId)
        : prodIndex{segId.prodIndex}
        , segOffset{segId.segOffset}
    {}

    SegId& operator=(const SegId& rhs)
    {
        prodIndex = rhs.prodIndex;
        segOffset = rhs.segOffset;
        return *this;
    }

    SegId& operator=(const SegId&& rhs)
    {
        prodIndex = rhs.prodIndex;
        segOffset = rhs.segOffset;
        return *this;
    }

    ProdIndex getProdIndex() const noexcept
    {
        return prodIndex;
    }

    ProdSize getOffset() const noexcept
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
class Peer;

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
    /**
     * Constructs. NB: Implicit construction.
     *
     * @param[in] prodIndex  Identifier of product
     */
    ChunkId(ProdIndex prodIndex)
        : isProd{true}
        , id{.prodIndex=prodIndex}
    {}

    /**
     * Constructs. NB: Implicit construction.
     *
     * @param[in] segId  Identifier of data-segment
     */
    ChunkId(const SegId segId)
        : isProd{false}
        , id{}
    {
        id.segId = segId;
    }

    ChunkId()
        : ChunkId(ProdIndex{})
    {}

    ChunkId(const ChunkId& chunkId)
        : isProd(chunkId.isProd)
        , id{}
    {
        if (isProd) {
            id.prodIndex = chunkId.id.prodIndex;
        }
        else {
            id.segId = chunkId.id.segId;
        }
    }

    ChunkId(const ChunkId&& chunkId)
        : isProd(chunkId.isProd)
        , id{}
    {
        if (isProd) {
            id.prodIndex = chunkId.id.prodIndex;
        }
        else {
            id.segId = chunkId.id.segId;
        }
    }

    ChunkId& operator=(const ChunkId& rhs)
    {
        if (rhs.isProd) {
            id.prodIndex = rhs.id.prodIndex;
        }
        else {
            id.segId = rhs.id.segId;
        }
        return *this;
    }

    ChunkId& operator=(const ChunkId&& rhs)
    {
        if (rhs.isProd) {
            id.prodIndex = rhs.id.prodIndex;
        }
        else {
            id.segId = rhs.id.segId;
        }
        return *this;
    }

    operator bool()
    {
        return !isProd || static_cast<bool>(id.prodIndex);
    }

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

    void request(Peer& peer) const;
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

    const SegId& getSegId() const noexcept
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

    ProdSize getSegOffset() const noexcept
    {
        return id.getOffset();
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
 * The data portion of a data-segment.
 */
class SegData
{
protected:
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    SegData(Impl*);

public:
    virtual ~SegData() noexcept;

    /**
     * Returns the size of the data-segment in bytes.
     *
     * @return  Size of the data-segment in bytes
     */
    SegSize getSegSize() const noexcept;

    std::string to_string() const;

    /**
     * Gets the data. <b>Should only be called once.</b>
     *
     * @param[in] buf     Destination for the data of at least `getSegSize()`
     *                    bytes
     */
    void getData(void* buf);
};

class MemSegData final : public SegData
{
    class Impl;

public:
    MemSegData(
            const void*   data,
            const SegSize segSize);

    const void* data() const noexcept;
};

class TcpSegData final : public SegData
{
    class Impl;

public:
    TcpSegData(
            TcpSock&      sock,
            const SegSize segSize);
};

class UdpSegData final : public SegData
{
    class Impl;

public:
    UdpSegData(
            UdpSock&      sock,
            const SegSize segSize);
};

/******************************************************************************/

/**
 * Abstract data-segment.
 */
class DataSeg
{
public:
    class Impl;

protected:
    std::shared_ptr<Impl> pImpl;

    DataSeg(Impl* impl);

public:
    DataSeg() =default;

    virtual ~DataSeg() noexcept =0;

    operator bool() const noexcept;

    const SegInfo& getSegInfo() const;

    const SegId& getSegId() const noexcept;

    SegSize getSegSize() const noexcept;

    ProdIndex getProdIndex() const noexcept;

    ProdSize getProdSize() const noexcept;

    ProdSize getSegOffset() const noexcept;

    std::string to_string() const;

    void getData(void* buf) const;
};

/******************************************************************************/

/**
 * Data-segment that resides in memory.
 */
class MemSeg final : public DataSeg
{
    class Impl;

public:
    MemSeg();

    MemSeg(const SegInfo& info,
           const void*    data);

    operator bool() const noexcept;

    const void* data() const;

    bool operator ==(const MemSeg& rhs) const noexcept;
};

/******************************************************************************/

/**
 * Data-segment from a UDP socket.
 */
class UdpSeg final : public DataSeg
{
    class Impl;

public:
    UdpSeg( const SegInfo& info,
            UdpSock&       sock);
};

/**
 * Data-segment from a TCP socket.
 */
class TcpSeg final : public DataSeg
{
    class Impl;

public:
    TcpSeg( const SegInfo& info,
            TcpSock&       sock);
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
