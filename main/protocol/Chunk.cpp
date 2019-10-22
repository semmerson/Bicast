/**
 * Chunk of arbitrary data.
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

#include <climits>
#include <functional>
#include <unistd.h>

namespace hycast {

static const ChunkId::Id  PRODINFO_MASK = (static_cast<ChunkId::Id>(1) <<
        ((sizeof(ChunkId::Id)*CHAR_BIT) - 1));
static const ChunkId::Id  SEGINDEX_MASK = ((1 << 24) - 1);

bool ChunkId::isProdInfo() const noexcept
{
    return id & PRODINFO_MASK;
}

bool ChunkId::operator==(const ChunkId rhs) const noexcept
{
    return id == rhs.id;
}

size_t ChunkId::hash() const noexcept
{
    return std::hash<Id>()(id);
}

std::string ChunkId::to_string() const
{
    return std::to_string(id);
}

void ChunkId::write(TcpSock& sock) const
{
    sock.write(id);
}

ChunkId ChunkId::read(TcpSock& sock)
{
    ChunkId chunkId;

    if (!sock.read(chunkId.id))
        throw EOF_ERROR();

    return chunkId;
}

SegIndex ChunkId::getSegIndex() const noexcept
{
    if (isProdInfo())
        throw LOGIC_ERROR("Chunk-ID isn't for a data-segment");

    return id & SEGINDEX_MASK;
}

/******************************************************************************/

class Chunk::Impl
{
protected:
    ChunkId   id;
    ChunkSize size;

    typedef struct {
        decltype(ChunkId::id) id;
        ChunkSize             size;
    } Header;
    const size_t HEADER_SIZE = sizeof(ChunkId::id) + sizeof(ChunkSize);

    Impl()
        : id{}
        , size{0}
    {}

    Impl(const ChunkId& id)
        : id{id}
        , size{0}
    {}

    Impl(   const ChunkId&  id,
            const ChunkSize size)
        : id{id}
        , size{size}
    {}

public:
    virtual ~Impl() noexcept =0;

    const ChunkId& getId() const noexcept
    {
        return id;
    }

    const ChunkSize getSize() const noexcept
    {
        return size;
    }

    SegIndex getSegIndex() const noexcept
    {
        return id.getSegIndex();
    }

    virtual void write(void* data) =0;
};

Chunk::Impl::~Impl() noexcept
{}

/******************************************************************************/

Chunk::Chunk()
    : pImpl{}
{}

Chunk::Chunk(Impl* const impl)
    : pImpl{impl}
{}

Chunk::~Chunk()
{}

Chunk::operator bool() const noexcept
{
    return (bool)pImpl;
}

const ChunkId& Chunk::getId() const noexcept
{
    return pImpl->getId();
}

ChunkSize Chunk::getSize() const noexcept
{
    return pImpl->getSize();
}

SegIndex Chunk::getSegIndex() const noexcept
{
    return pImpl->getSegIndex();
}

void Chunk::write(void* data)
{
    pImpl->write(data);
}

/******************************************************************************/

class MemChunk::Impl final : public Chunk::Impl
{
private:
    const void* data;

public:
    Impl(   const ChunkId&  id,
            const ChunkSize size,
            const void*     data)
        : Chunk::Impl(id, size)
        , data{data}
    {}

    const void* getData()
    {
        return data;
    }

    void write(TcpSock& sock) const
    {
        id.write(sock);
        sock.write(size);
        sock.write(data, size);
    }

    void write(UdpSndrSock& sock) const
    {
        struct iovec iov[3];

        auto chunkId = sock.hton(id.id);
        iov[0].iov_base = &chunkId;
        iov[0].iov_len = sizeof(chunkId);

        auto chunkSize = sock.hton(size);
        iov[1].iov_base = &chunkSize;
        iov[1].iov_len = sizeof(chunkSize);

        iov[2].iov_base = const_cast<void*>(data); // Safe cast
        iov[2].iov_len = size;

        sock.write(iov, 3);
    }

    void write(void* data)
    {
        ::memcpy(data, this->data, size);
    }
};

/******************************************************************************/

MemChunk::MemChunk(
        const ChunkId&  id,
        const ChunkSize size,
        const void*     data)
    : Chunk{new Impl(id, size, data)}
{}

const void* MemChunk::getData() const
{
    return static_cast<Impl*>(pImpl.get())->getData();
}

void MemChunk::write(TcpSock& sock) const
{
    static_cast<Impl*>(pImpl.get())->write(sock);
}

void MemChunk::write(UdpSndrSock& sock) const
{
    static_cast<Impl*>(pImpl.get())->write(sock);
}

/******************************************************************************/

class TcpChunk::Impl final : public Chunk::Impl
{
    TcpSock sock;

public:
    /**
     * Constructs.
     *
     * @param[in] id                     Chunk ID
     * @throws    EofError               EOF
     * @throws    SystemError            Error reading chunk's header
     */
    Impl(TcpSock& sock)
        : Chunk::Impl()
        , sock{sock}
    {
        if (!sock.read(id.id) || !sock.read(size))
            throw EOF_ERROR("Couldn't read chunk's header");
    }

    void write(void* data)
    {
        const size_t nread = sock.read(data, size);

        if (nread != size)
            throw RUNTIME_ERROR("Could only read " + std::to_string(nread) +
                    " bytes out of " + std::to_string(size));
    }
};

/******************************************************************************/

TcpChunk::TcpChunk()
    : Chunk{}
{}

TcpChunk::TcpChunk(TcpSock& sock)
    : Chunk{new Impl(sock)}
{}

void TcpChunk::read(void* data)
{
    static_cast<Impl*>(pImpl.get())->write(data);
}

/******************************************************************************/

class UdpChunk::Impl final : public Chunk::Impl
{
private:
    UdpRcvrSock sock;

public:
    /**
     * Constructs from a UDP socket.
     *
     * @param[in] sock                   UDP socket from which the chunk data
     *                                   can be read
     * @throws    std::invalid_argument  Socket isn't UDP
     * @throws    std::EofError          EOF
     * @throws    std::system_error      Error reading Chunk's header
     */
    Impl(UdpRcvrSock& sock)
        : Chunk::Impl()
        , sock{sock}
    {
        Chunk::Impl::Header header;

        // TODO: Make this more robust by accommodating padding in `header`
        if (!sock.peek(&header, Chunk::Impl::HEADER_SIZE))
            throw EOF_ERROR("Couldn't peek at chunk");

        id = ChunkId{sock.ntoh(header.id)};
        size = sock.ntoh(header.size);
    }

    void write(void* data)
    {
        Chunk::Impl::Header header;
        struct iovec        iov[2];

        iov[0].iov_base = &header;
        iov[0].iov_len = Chunk::Impl::HEADER_SIZE;

        iov[1].iov_base = const_cast<void*>(data); // Safe cast
        iov[1].iov_len = size;

        if (sock.read(iov, 2) != iov[0].iov_len + iov[1].iov_len)
            throw EOF_ERROR();

        header.id = sock.ntoh(header.id);
        if (header.id != id.id)
            throw LOGIC_ERROR("Chunk IDs don't match: expected: " +
                    id.to_string() + ", actual: " + std::to_string(header.id));
        header.size = sock.ntoh(header.size);
        if (header.size != size)
            throw LOGIC_ERROR("Chunk sizes don't match: expected: " +
                    std::to_string(size) + ", actual: " +
                    std::to_string(header.size));
    }
};

/******************************************************************************/

UdpChunk::UdpChunk()
    : Chunk{}
{}

UdpChunk::UdpChunk(UdpRcvrSock& sock)
    : Chunk{new Impl(sock)}
{}

void UdpChunk::read(void* data)
{
    static_cast<Impl*>(pImpl.get())->write(data);
}

} // namespace
