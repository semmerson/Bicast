/**
 * Serializes and deserializes objects and values.
 *
 * Copyright 2019 University Corporation for Atmospheric Research. All Rights
 * reserved. See file "COPYING" in the top-level source-directory for usage
 * restrictions.
 *
 *        File: Wire.cpp
 *  Created on: May 21, 2019
 *      Author: Steven R. Emmerson
 */

#include "config.h"

#include "error.h"
#include "Socket.h"
#include "Wire.h"

#include <cstdio>

namespace hycast {

class Wire::Impl
{
public:
    Impl() noexcept =default;

    virtual ~Impl() =0;

    virtual void serialize(const uint8_t value) =0;

    virtual void serialize(const uint16_t value) =0;

    virtual void serialize(const uint32_t value) =0;

    virtual void serialize(const uint64_t value) =0;

    virtual void serialize(const std::string string) =0;

    virtual void serialize(const void* data, const size_t nbytes) =0;

    virtual void flush() =0;

    virtual void deserialize(uint8_t& value) =0;

    virtual void deserialize(uint16_t& value) =0;

    virtual void deserialize(uint32_t& value) =0;

    virtual void deserialize(uint64_t& value) =0;

    virtual void deserialize(std::string& string) =0;

    virtual size_t deserialize(void* const data, const size_t nbytes) =0;
};

Wire::Impl::~Impl()
{}

/******************************************************************************/

Wire::Wire::~Wire()
{}

/******************************************************************************/

class NetWire final : public Wire::Impl
{
private:
    static const int MAX_VEC = 10;
    Socket           sock;
    struct iovec     iov[MAX_VEC];
    int              iovCnt;
    union buf {
        uint64_t         u64;
        uint32_t         u32[2];
        uint16_t         u16;
        uint8_t          u8;
    }                buf[MAX_VEC];
    union buf*       next;

    inline void checkBuf()
    {
        if (next >= buf + MAX_VEC)
            throw std::range_error("More than " + std::to_string(MAX_VEC) +
                    " elements");
    }

public:
    NetWire(Socket& sock)
        : sock{sock}
        , iov{}
        , iovCnt{0}
        , buf{0}
        , next{buf}
    {}

    NetWire(Socket&& sock)
        : sock{sock}
        , iov{}
        , iovCnt{0}
        , buf{0}
        , next{buf}
    {}

    void serialize(const uint8_t value)
    {
        checkBuf();

        next->u8 = value;
        iov[iovCnt].iov_base = next++;
        iov[iovCnt++].iov_len = sizeof(value);
    }

    void serialize(const uint16_t value)
    {
        LOG_DEBUG("value: %hu", value);
        checkBuf();

        next->u16 = htons(value);
        iov[iovCnt].iov_base = next++;
        iov[iovCnt++].iov_len = sizeof(value);
    }

    void serialize(const uint32_t value)
    {
        checkBuf();

        next->u32[0] = htonl(value);
        iov[iovCnt].iov_base = next++;
        iov[iovCnt++].iov_len = sizeof(value);
    }

    void serialize(const uint64_t value)
    {
        checkBuf();

        next->u32[0] = htonl(static_cast<uint32_t>(value >> 32));
        next->u32[1] = htonl(static_cast<uint32_t>(value));
        iov[iovCnt].iov_base = next++;
        iov[iovCnt++].iov_len = sizeof(value);
    }

    void serialize(const void* data, const size_t nbytes)
    {
        checkBuf();

        iov[iovCnt].iov_base = const_cast<void*>(data);
        iov[iovCnt++].iov_len = nbytes;
    }

    void serialize(const std::string string)
    {
        auto nbytes = string.size();

        serialize(nbytes);
        serialize(string.data(), nbytes);
    }

    void flush()
    {
        sock.writev(iov, iovCnt);
        iovCnt = 0;
        next = buf;
    }

    void deserialize(uint8_t& value)
    {
        if (sock.read(&value, sizeof(value)) != sizeof(value))
            throw RUNTIME_ERROR("EOF");
    }

    void deserialize(uint16_t& value)
    {
        uint16_t val;

        if (sock.read(&val, sizeof(val)) != sizeof(val))
            throw RUNTIME_ERROR("EOF");

        value = ntohs(val);
        LOG_DEBUG("value: %hu", value);
    }

    void deserialize(uint32_t& value)
    {
        uint32_t val;

        if (sock.read(&val, sizeof(val)) != sizeof(val))
            throw RUNTIME_ERROR("EOF");

        value = ntohl(val);
    }

    void deserialize(uint64_t& value)
    {
        uint32_t val[2];

        if (sock.read(&val, sizeof(val)) != sizeof(val))
            throw RUNTIME_ERROR("EOF");

        value = (static_cast<uint64_t>(ntohl(val[0])) << 32) | ntohl(val[1]);
    }

    size_t deserialize(void* const data, const size_t nbytes)
    {
        return sock.read(data, nbytes);
    }

    void deserialize(std::string& string)
    {
        std::string::size_type nbytes;

        deserialize(nbytes);

        char data[nbytes+1];

        if (deserialize(data, nbytes) != nbytes)
            throw RUNTIME_ERROR("EOF");

        data[nbytes] = 0;
        string = std::string(data);
    }
};

/******************************************************************************/

Wire::Wire()
    : pImpl{}
{}

Wire::Wire(Impl* const impl)
    : pImpl{impl}
{}

Wire::Wire(Socket& sock)
    : pImpl{new NetWire(sock)}
{}

Wire::Wire(Socket&& sock)
    : pImpl{new NetWire(sock)}
{}

void Wire::serialize(const uint8_t value)
{
    pImpl->serialize(value);
}

void Wire::serialize(const uint16_t value)
{
    pImpl->serialize(value);
}

void Wire::serialize(const uint32_t value)
{
    pImpl->serialize(value);
}

void Wire::serialize(const uint64_t value)
{
    pImpl->serialize(value);
}

void Wire::serialize(const void* data, const size_t nbytes)
{
    pImpl->serialize(data, nbytes);
}

void Wire::serialize(const std::string string)
{
    pImpl->serialize(string);
}

void Wire::flush()
{
    pImpl->flush();
}

void Wire::deserialize(uint8_t& value)
{
    pImpl->deserialize(value);
}

void Wire::deserialize(uint16_t& value)
{
    pImpl->deserialize(value);
}

void Wire::deserialize(uint32_t& value)
{
    pImpl->deserialize(value);
}

void Wire::deserialize(uint64_t& value)
{
    pImpl->deserialize(value);
}

size_t Wire::deserialize(void* const data, const size_t nbytes)
{
    return pImpl->deserialize(data, nbytes);
}

void Wire::deserialize(std::string& string)
{
    pImpl->deserialize(string);
}

} // namespace
