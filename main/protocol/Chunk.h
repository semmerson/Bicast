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

    std::string to_string() const;

    void write(TcpSock& sock) const;

    /**
     * Constructs an instance from a TCP socket.
     *
     * @param[in] sock         TCP socket
     * @return                 Chunk ID read from socket
     * @throws    EofError     EOF
     * @throws    SystemError  Read failure
     */
    static ChunkId read(TcpSock& sock);
};

/******************************************************************************/

typedef uint16_t              ChunkSize;

/**
 * A chunk of data.
 */
class Chunk
{
public:
    class Impl;

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

/**
 * Chunk whose data resides in memory.
 */
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

    void write(TcpSock& sock) const;
};

/******************************************************************************/

/**
 * Chunk whose data must be read from a socket.
 */
class InetChunk : public Chunk
{
protected:
    class Impl;

    InetChunk(Impl* impl);

public:
    InetChunk() =default;

    virtual ~InetChunk() =0;

    /**
     * Reads the chunk's data.
     *
     * @param[out] data                Buffer for the chunk's data
     * @throws     std::runtime_error  All the chunk's data couldn't be read
     * @threadsafety                   Compatible but unsafe
     * @exceptionsafety                Basic guarantee
     * @cancellationpoint              Yes
     */
    virtual void read(void* data) =0;
};

/******************************************************************************/

/**
 * Chunk whose data must be read from a TCP socket.
 */
class TcpChunk final : public InetChunk
{
    class Impl;

public:
    TcpChunk();

    /**
     * Constructs from a TCP socket.
     *
     * @param[in] sock                   TCP socket
     * @throws    EOF_ERROR("Couldn't peek at chunk");
     */
    TcpChunk(TcpSock& sock);

    /**
     * Reads the chunk's data.
     *
     * @param[out] data                Buffer for the chunk's data
     * @throws     SystemError         I/O error
     * @throws     RuntimeError        Couldn't read chunk's data
     * @threadsafety                   Compatible but unsafe
     * @exceptionsafety                Basic guarantee
     * @cancellationpoint              Yes
     */
    void read(void* data);
};

/******************************************************************************/

/**
 * Chunk whose data must be read from a UDP socket.
 */
class UdpChunk final : public InetChunk
{
    class Impl;

public:
    UdpChunk();

    /**
     * Constructs from a UDP socket.
     *
     * @param[in] sock                   UDP socket
     * @throws    std::system_error      Chunk's header couldn't be read from
     *                                   socket
     */
    UdpChunk(UdpRcvrSock& sock);

    /**
     * Reads the chunk's data.
     *
     * @param[out] data                Buffer for the chunk's data
     * @throws     EofError            EOF
     * @throws     SystemError         I/O failure
     * @throws     LogicError          Logic error
     * @threadsafety                   Compatible but unsafe
     * @exceptionsafety                Basic guarantee
     * @cancellationpoint              Yes
     */
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
