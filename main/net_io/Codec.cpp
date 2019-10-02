/**
 * Supports the remote procedure calls of this package
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Codec.cpp
 *  Created on: May 21, 2019
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "Codec.h"
#include "error.h"
#include "Socket.h"

#include <cstdio>

namespace hycast {

class Codec::Impl
{
protected:
    Socket sock;

    inline uint16_t hton(const uint16_t value)
    {
        return htons(value);
    }

    inline uint32_t hton(const uint32_t value)
    {
        return htonl(value);
    }

    inline uint64_t hton(uint64_t value)
    {
        uint64_t  v64;
        uint32_t* v32 = reinterpret_cast<uint32_t*>(&v64);

        v32[0] = hton(static_cast<uint32_t>(value >> 32));
        v32[1] = hton(static_cast<uint32_t>(value));

        return v64;
    }

    inline uint16_t ntoh(const uint16_t value)
    {
        return ntohs(value);
    }

    inline uint32_t ntoh(const uint32_t value)
    {
        return ntohl(value);
    }

    inline uint64_t ntoh(uint64_t value)
    {
        uint32_t* v32 = reinterpret_cast<uint32_t*>(&value);

        return (static_cast<uint64_t>(ntoh(v32[0])) << 32) | ntoh(v32[1]);
    }

public:
    Impl(Socket& sock)
        : sock{sock}
    {}

    Impl(Socket&& sock)
        : sock{sock}
    {}
};

/******************************************************************************/

class StreamCodec::Impl : public Codec::Impl
{
    void encode(uint64_t value)
    {
        value = hton(value);
        sock.write(&value, sizeof(value));
    }

    void read(void* buf, size_t nbytes)
    {
        char* next = static_cast<char*>(buf);

        do {
            size_t nread = sock.read(next, nbytes);

            if (nread == 0)
                throw RUNTIME_ERROR("EOF");

            nbytes -= nread;
            next += nread;
        } while (nbytes);
    }

    void decode(uint64_t& value)
    {
        read(&value, sizeof(value));
        value = ntoh(value);
    }

public:
    Impl(Socket& sock)
        : Codec::Impl{sock}
    {}

    Impl(Socket&& sock)
        : Codec::Impl{sock}
    {}

    void encode(in_port_t value)
    {
        value = hton(value);
        sock.write(&value, sizeof(value));
    }

    void encode(const ChunkId& id)
    {
        encode(id.id);
    }

    void encode(const MemChunk& chunk)
    {
        struct iovec iov[3];

        auto id = hton(chunk.getId().id);
        iov[0].iov_base = &id;
        iov[0].iov_len = sizeof(id);

        auto size = chunk.getSize();
        iov[2].iov_base = const_cast<void*>(chunk.getData()); // Safe cast
        iov[2].iov_len = size;

        size = hton(size);
        iov[1].iov_base = &size;
        iov[1].iov_len = sizeof(size);

        sock.writev(iov, 3);
    }

    void encode(const void* data, const size_t nbytes)
    {
        return sock.write(data, nbytes);
    }

    void decode(uint16_t& value)
    {
        read(&value, sizeof(value));
        value = ntoh(value);
    }

    void decode(ChunkId& chunkId)
    {
        decltype(ChunkId::id) id;

        decode(id);
        chunkId = ChunkId(id);
    }

    void decode(StreamChunk& chunk)
    {
        ChunkId id;
        decode(id);

        ChunkSize size;
        decode(size);

        chunk = StreamChunk(id, size, sock);
    }

    size_t decode(void* const data, const size_t nbytes)
    {
        return sock.read(data, nbytes);
    }
};

/******************************************************************************/

Codec::Codec()
    : pImpl{}
{}

Codec::Codec(Impl* const impl)
    : pImpl{impl}
{}

Codec::Codec(Socket& sock)
    : pImpl{new StreamCodec::Impl(sock)}
{}

Codec::Codec(Socket&& sock)
    : pImpl{new StreamCodec::Impl(sock)}
{}

/******************************************************************************/

StreamCodec::StreamCodec()
    : Codec{}
{}

StreamCodec::StreamCodec(Socket& sock)
    : Codec{new StreamCodec::Impl(sock)}
{}

StreamCodec::StreamCodec(Socket&& sock)
    : Codec{new StreamCodec::Impl(sock)}
{}

void StreamCodec::encode(const in_port_t port) const
{
    (static_cast<Impl*>(pImpl.get()))->encode(port);
}

void StreamCodec::encode(const ChunkId& chunkId) const
{
    (static_cast<Impl*>(pImpl.get()))->encode(chunkId);
}

void StreamCodec::encode(const MemChunk& chunk) const
{
    (static_cast<Impl*>(pImpl.get()))->encode(chunk);
}

void StreamCodec::encode(const void* data, const size_t nbytes) const
{
    (static_cast<Impl*>(pImpl.get()))->encode(data, nbytes);
}

void StreamCodec::decode(in_port_t& port) const
{
    (static_cast<Impl*>(pImpl.get()))->decode(port);
}

void StreamCodec::decode(ChunkId& chunkId) const
{
    (static_cast<Impl*>(pImpl.get()))->decode(chunkId);
}

void StreamCodec::decode(StreamChunk& chunk) const
{
    (static_cast<Impl*>(pImpl.get()))->decode(chunk);
}

size_t StreamCodec::decode(void* const data, const size_t nbytes) const
{
    return (static_cast<Impl*>(pImpl.get()))->decode(data, nbytes);
}

} // namespace
