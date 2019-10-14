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
#include "unistd.h"

#include <climits>
#include <functional>

namespace hycast {

bool ChunkId::operator==(const ChunkId rhs) const noexcept
{
    return id == rhs.id;
}

size_t ChunkId::hash() const noexcept
{
    return std::hash<decltype(id)>()(id);
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

const ChunkId& Chunk::getId() const noexcept
{
    return pImpl->getId();
}

ChunkSize Chunk::getSize() const noexcept
{
    return pImpl->getSize();
}

Chunk::operator bool() const noexcept
{
    return (bool)pImpl;
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
    static_cast<Impl*>(pImpl.get())->getData();
}

void MemChunk::write(TcpSock& sock) const
{
    static_cast<Impl*>(pImpl.get())->write(sock);
}

/******************************************************************************/

class InetChunk::Impl : public Chunk::Impl
{
protected:
    Impl()
        : Chunk::Impl()
    {}

    /**
     * Constructs.
     *
     * @param[in] id                     Chunk ID
     * @param[in] size                   Size of chunk's data in bytes
     * @throws    std::invalid_argument  Socket isn't a byte-stream
     */
    Impl(   const ChunkId&  id,
            const ChunkSize size)
        : Chunk::Impl(id, size)
    {}

public:
    virtual ~Impl()
    {}

    virtual void read(void* data) =0;
};

InetChunk::InetChunk(Impl* impl)
    : Chunk(impl)
{}

InetChunk::~InetChunk()
{}

/******************************************************************************/

/******************************************************************************/

class TcpChunk::Impl final : public InetChunk::Impl
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
        : InetChunk::Impl()
        , sock{sock}
    {
        if (!sock.read(id.id) || !sock.read(size))
            throw EOF_ERROR("Couldn't read chunk's header");
    }

    void read(void* data)
    {
        const size_t nread = sock.read(data, size);

        if (nread != size)
            throw RUNTIME_ERROR("Could only read " + std::to_string(nread) +
                    " bytes out of " + std::to_string(size));
    }
};

/******************************************************************************/

TcpChunk::TcpChunk()
    : InetChunk{}
{}

TcpChunk::TcpChunk(TcpSock& sock)
    : InetChunk{new Impl(sock)}
{}

void TcpChunk::read(void* data)
{
    static_cast<Impl*>(pImpl.get())->read(data);
}

/******************************************************************************/

class UdpChunk::Impl final : public InetChunk::Impl
{
private:
    UdpRcvrSock sock;

public:
    /**
     * Constructs from a record-oriented socket.
     *
     * @param[in] sock                   Record-oriented socket from which the
     *                                   chunk data can be read
     * @throws    std::invalid_argument  Socket isn't record-oriented
     * @throws    std::EofError          EOF
     * @throws    std::system_error      Error reading Chunk's header
     */
    Impl(UdpRcvrSock& sock)
        : InetChunk::Impl()
        , sock{sock}
    {
        Chunk::Impl::Header header;

        // TODO: Make this more robust by accommodating padding in `header`
        if (!sock.peek(&header, Chunk::Impl::HEADER_SIZE))
            throw EOF_ERROR("Couldn't peek at chunk");

        id = ChunkId(sock.ntoh(header.id));
        size = sock.ntoh(header.size);
    }

    void read(void* data)
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
    : InetChunk{}
{}

UdpChunk::UdpChunk(UdpRcvrSock& sock)
    : InetChunk{new Impl(sock)}
{}

void UdpChunk::read(void* data)
{
    static_cast<Impl*>(pImpl.get())->read(data);
}

} // namespace
