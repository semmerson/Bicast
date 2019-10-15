/**
 * Supports the remote procedure calls of this package
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All rights
 * reserved. See the file COPYING in the top-level source-directory for
 * licensing conditions.
 *
 *   @file: Codec.h
 * @author: Steven R. Emmerson
 */

#ifndef MAIN_RPC_RPC_H_
#define MAIN_RPC_RPC_H_

#include <main/protocol/Chunk.h>
#include "Socket.h"

#include <memory>

namespace hycast {

/**
 * Supports the remote procedure calls of this package
 */
class Codec
{
protected:
    class Impl;

    std::shared_ptr<Impl> pImpl;

    /**
     * Constructs from an implementation.
     *
     * @param[in] impl  The implementation
     */
    Codec(Impl* impl);

public:
    /**
     * Default Constructs.
     */
    Codec();

    /**
     * Constructs from a socket.
     *
     * @param[in] sock  Socket
     */
    Codec(TcpSock& sock);

    /**
     * Move constructs from a socket.
     *
     * @param[in] sock  Socket
     */
    Codec(TcpSock&& sock);

    virtual void encode(const MemChunk& chunk) const =0;
};

class StreamCodec final : public Codec
{
public:
    class Impl;

    StreamCodec();

    StreamCodec(TcpSock& sock);

    StreamCodec(TcpSock&& sock);

    void encode(const in_port_t port) const;

    void encode(const ChunkId& chunkId) const;

    void encode(const MemChunk& chunk) const override;

    void encode(const void* data, const size_t nbytes) const;

    void decode(in_port_t& port) const;

    void decode(ChunkId& chunkId) const;

    void decode(TcpChunk& chunk) const;

    size_t decode(void* data, const size_t nbytes) const;
};

class RecordCodec final : public Codec
{
public:
    class Impl;

    RecordCodec();

    RecordCodec(UdpSock& sock);

    RecordCodec(UdpSock&& sock);

    void encode(const MemChunk& chunk) const override;

    void decode(UdpChunk& chunk) const;
};

} // namespace

#endif /* MAIN_RPC_RPC_H_ */
