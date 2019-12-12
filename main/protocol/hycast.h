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
typedef uint32_t ProdIndex;
typedef uint32_t ProdSize;

/******************************************************************************/

class ProdInfo
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    ProdInfo(Impl* const impl);

public:
    /**
     * Constructs.
     *
     * @param[in] index  Product index
     * @param[in] size   Product size in bytes
     * @param[in] name   Product name
     */
    ProdInfo(
            ProdIndex          index,
            ProdSize           size,
            const std::string& name);

    operator bool()
    {
        return (bool)pImpl;
    }

    ProdIndex getIndex() const;

    ProdSize getSize() const;

    const std::string& getName() const;

    bool operator ==(const ProdInfo& rhs) const;
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
        : SegId(0, 0)
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
        return std::hash<ProdIndex>()(prodIndex) ^
                std::hash<ProdSize>()(segOffset);
    }

    bool operator ==(const SegId& rhs) const noexcept
    {
        return prodIndex == rhs.prodIndex &&
                segOffset == rhs.segOffset;
    }

    std::string to_string() const;
};

class ChunkNotice
{
protected:
    ProdIndex prodIndex;
    ProdSize  segOffset;

public:
    ChunkNotice(
            const ProdIndex prodIndex,
            const ProdSize  segOffset)
        : prodIndex{prodIndex}
        , segOffset{segOffset}
    {}

    virtual ~ChunkNotice();

    ProdIndex getProdIndex() const noexcept
    {
        return prodIndex;
    }

    size_t hash() const noexcept
    {
        return std::hash<ProdIndex>()(prodIndex) ^
                std::hash<ProdSize>()(segOffset);
    }

    bool operator ==(const ChunkNotice& rhs) const noexcept
    {
        return prodIndex == rhs.prodIndex &&
                segOffset == rhs.segOffset;
    }

    virtual std::string to_string() const =0;
};

class ProdNotice final : public ChunkNotice
{
public:
    ProdNotice(const ProdIndex prodIndex)
        : ChunkNotice{prodIndex, (ProdSize)-1}
    {}

    std::string to_string() const
    {
        return std::to_string(prodIndex);
    }
};

class SegNotice final : public ChunkNotice
{
public:
    SegNotice(const SegId& segId)
        : ChunkNotice{segId.getProdIndex(), segId.getSegOffset()}
    {}

    std::string to_string() const
    {
        return "{prodIndex: " + std::to_string(prodIndex) + ", segOffset: " +
                std::to_string(segOffset) + "}";
    }
};

typedef ChunkNotice ChunkRequest;

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
class MemSeg
{
    class                 Impl;
    std::shared_ptr<Impl> pImpl;

    MemSeg(Impl* impl);

public:
    MemSeg(const SegInfo& info,
           const void*    data);

    operator bool()
    {
        return (bool)pImpl;
    }

    const SegInfo& getInfo() const;

    const void* getData() const;

    SegSize getSegSize() const;

    ProdIndex getProdIndex() const;

    ProdSize getProdSize() const;

    ProdSize getSegOffset() const;
};

/******************************************************************************/

/**
 * Abstract data-segment from a socket
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

    virtual void write(void* data) const =0;
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

    void write(void* data) const override;
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

    void write(void* data) const override;
};

} // namespace

/******************************************************************************/

namespace std {
    template<>
    struct hash<hycast::SegId>
    {
        size_t operator ()(const hycast::SegId& id) const
        {
            return id.hash();
        }
    };
}

#endif /* MAIN_HYCAST_H_ */
