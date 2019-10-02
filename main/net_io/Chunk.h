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

#ifndef MAIN_RPC_CHUNK_H_
#define MAIN_RPC_CHUNK_H_

#include "Socket.h"

#include <memory>

/******************************************************************************/

namespace hycast {

class ChunkId
{
private:
    friend std::hash<ChunkId>;
    friend std::equal_to<ChunkId>;

public:
    uint64_t id;

    ChunkId()
        : id{0}
    {}

    ChunkId(const uint64_t id)
        : id{id}
    {}

    bool operator ==(const ChunkId rhs) const noexcept;

    size_t hash() const noexcept;
};

/******************************************************************************/

typedef uint16_t              ChunkSize;

class Chunk
{
public:
    friend class Codec;
    class        Impl;

protected:
    std::shared_ptr<Impl> pImpl;

    Chunk(Impl* const impl);

public:
    Chunk();

    virtual ~Chunk() =0;

    const ChunkId& getId() const noexcept;

    ChunkSize getSize() const noexcept;

    operator bool() const noexcept;
};

/******************************************************************************/

typedef std::shared_ptr<void> DataPtr;

class MemChunk final : public Chunk
{
private:
    class Impl;

public:
    MemChunk(
            const ChunkId&  id,
            const ChunkSize size,
            const void*     data);

    const void* getData() const;
};

/******************************************************************************/

class StreamChunk final : public Chunk
{
public:
    class Impl;

public:
    StreamChunk();

    /**
     * Constructs.
     *
     * @param[in] rpc  RPC module from which the chunk can be deserialized.
     */
    StreamChunk(
            const ChunkId&  id,
            const ChunkSize size,
            Socket&         sock);

    void read(void* data);
};

/******************************************************************************/

class RecordChunk final : public Chunk
{
public:
    class Impl;

public:
    /**
     * Constructs.
     *
     * @param[in] sock  Socket from which the chunk can be read.
     */
    RecordChunk(Socket& sock);

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

#endif /* MAIN_RPC_CHUNK_H_ */
