/**
 * A chunk of data.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "Copying" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Chunk.h
 *  Created on: May 10, 2019
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_PEER_CHUNK_H_
#define MAIN_PEER_CHUNK_H_

#include "Serializable.h"
#include "Wire.h"

#include <memory>

/******************************************************************************/

namespace hycast {

class ChunkId : public Serializable
{
private:
    friend std::hash<ChunkId>;
    friend std::equal_to<ChunkId>;

public:
    uint64_t id;

    ChunkId(const uint64_t id)
        : id{id}
    {}

    void write(Wire& wire) const;

    static ChunkId read(Wire& wire);

    bool operator ==(const ChunkId rhs) const noexcept;

    size_t hash() const noexcept;
};

/******************************************************************************/

typedef uint16_t              ChunkSize;

class Chunk
{
public:
    class Impl;

protected:
    std::shared_ptr<Impl> pImpl;

    Chunk(Impl* const impl);

public:
    virtual ~Chunk() =0;

    const ChunkId& getId() const noexcept;

    ChunkSize getSize() const noexcept;

    operator bool() const noexcept;
};

/******************************************************************************/

typedef std::shared_ptr<void> DataPtr;

class MemChunk final : public Chunk, public Serializable
{
private:
    class Impl;

public:
    MemChunk(
            const ChunkId&  id,
            const ChunkSize size,
            const void*     data);

    void write(Wire& wire) const;
};

/******************************************************************************/

class WireChunk final : public Chunk
{
public:
    class Impl;

public:
    /**
     * Constructs.
     *
     * @param[in] wire  Wire from which the chunk can be deserialized.
     */
    WireChunk(Wire& wire);

    void read(void* data);
};

} // namespace

/******************************************************************************/

namespace std {
    template<>
    struct hash<hycast::ChunkId>
    {
        size_t operator ()(const hycast::ChunkId chunkId) const
        {
            return std::hash<decltype(chunkId.id)>()(chunkId.id);
        }
    };

    template<>
    struct equal_to<hycast::ChunkId>
    {
        size_t operator ()(
                const hycast::ChunkId id1,
                const hycast::ChunkId id2) const
        {
            return id1.id == id2.id;
        }
    };
}

#endif /* MAIN_PEER_CHUNK_H_ */
