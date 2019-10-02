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

namespace hycast {

bool ChunkId::operator==(const ChunkId rhs) const noexcept
{
    return id == rhs.id;
}

size_t ChunkId::hash() const noexcept
{
    return std::hash<decltype(id)>()(id);
}

/******************************************************************************/

class Chunk::Impl
{
protected:
    ChunkId   id;
    ChunkSize size;

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

/******************************************************************************/

class StreamChunk::Impl final : public Chunk::Impl
{
private:
    Socket sock;

public:
    /**
     * Constructs.
     *
     * @param[in] id    Chunk ID
     * @param[in] size  Size of chunk's data in bytes
     * @param[in] sock  Socket from which the chunk's data can be read
     */
    Impl(   const ChunkId&  id,
            const ChunkSize size,
            Socket&         sock)
        : Chunk::Impl(id, size)
        , sock{sock}
    {}

    void read(void* data)
    {
        sock.read(data, size);
    }
};

/******************************************************************************/

StreamChunk::StreamChunk()
    : Chunk{}
{}

StreamChunk::StreamChunk(
        const ChunkId&  id,
        const ChunkSize size,
        Socket&         sock)
    : Chunk{new Impl(id, size, sock)}
{}

void StreamChunk::read(void* data)
{
    static_cast<Impl*>(pImpl.get())->read(data);
}

} // namespace
