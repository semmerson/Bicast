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

void ChunkId::write(Wire& wire) const
{
    wire.serialize(id);
}

/**
 * Reads a chunk identifier from a wire.
 *
 * @param[in] wire               Wire from which to read the chunk ID
 * @throw     std::system_error  Couldn't read from the wire
 */
ChunkId ChunkId::read(Wire& wire)
{
    uint64_t id;

    wire.deserialize(id);
    return ChunkId(id);
}

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

class MemChunk::Impl final : public Chunk::Impl, public Serializable
{
private:
    const void* data;

public:
    Impl(
            const ChunkId&  id,
            const ChunkSize size,
            const void*     data)
        : Chunk::Impl(id, size)
        , data{data}
    {}

    void write(Wire& wire) const
    {
        id.write(wire);
        wire.serialize(size);
        wire.serialize(data, size);
    }
};

/******************************************************************************/

MemChunk::MemChunk(
        const ChunkId&  id,
        const ChunkSize size,
        const void*     data)
    : Chunk{new Impl(id, size, data)}
{}

void MemChunk::write(Wire& wire) const
{
    static_cast<Impl*>(pImpl.get())->write(wire);
}

/******************************************************************************/

class WireChunk::Impl final : public Chunk::Impl
{
private:
    Wire wire;

protected:
    void serializeData(Wire& wire) const
    {
        throw RUNTIME_ERROR("A WireChunk cannot be serialized");
    }

public:
    /**
     * Constructs.
     *
     * @param[in] wire  Wire from which the chunk can be deserialized.
     *
     */
    Impl(Wire& wire)
        : Chunk::Impl(ChunkId::read(wire))
        , wire{wire}
    {
        wire.deserialize(size);
    }

    void read(void* data)
    {
        wire.deserialize(data, size);
    }
};

/******************************************************************************/

WireChunk::WireChunk(Wire& wire)
    : Chunk{new Impl(wire)}
{}

void WireChunk::read(void* data)
{
    static_cast<Impl*>(pImpl.get())->read(data);
}

} // namespace
